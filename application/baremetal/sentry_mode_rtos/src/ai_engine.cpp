/******************************************************************************
 * @file    ai_engine.cpp
 * @brief   AI推理引擎 - TFLM 实现（SSD MobileNet v2 FPN-lite）
 ******************************************************************************/

#include "sentry_mode.h"
#include "ssd_mobilenet_v2_fpnlite_035_224_int8.h"

#include "tensorflow/lite/micro/micro_interpreter.h"
#include "tensorflow/lite/micro/micro_mutable_op_resolver.h"
#include "tensorflow/lite/micro/micro_log.h"
#include "tensorflow/lite/schema/schema_generated.h"

extern unsigned char ___pretrained_models_coco_2017_person_ssd_mobilenet_v2_fpnlite_035_224_ssd_mobilenet_v2_fpnlite_035_224_int8_tflite[1573512];
extern unsigned int ___pretrained_models_coco_2017_person_ssd_mobilenet_v2_fpnlite_035_224_ssd_mobilenet_v2_fpnlite_035_224_int8_tflite_len;

namespace {
    const tflite::Model* model = nullptr;
    tflite::MicroInterpreter* interpreter = nullptr;
    TfLiteTensor* input = nullptr;
    TfLiteTensor* output = nullptr;
    
    constexpr int kTensorArenaSize = 1024 * 1024;
    static uint8_t tensor_arena[kTensorArenaSize];
    
    static int s_current_mode = 0;
    static bool s_ai_initialized = false;
    static int s_last_result = 0;
    static float s_last_confidence = 0.0f;
    
    // 检测框缓冲区
    static DetectionBox s_detected_boxes[10];
    static int s_num_detected_boxes = 0;
}

static float iou(DetectionBox a, DetectionBox b) {
    int inter_x_min = (a.x_min > b.x_min) ? a.x_min : b.x_min;
    int inter_y_min = (a.y_min > b.y_min) ? a.y_min : b.y_min;
    int inter_x_max = (a.x_max < b.x_max) ? a.x_max : b.x_max;
    int inter_y_max = (a.y_max < b.y_max) ? a.y_max : b.y_max;

    int inter_w = inter_x_max - inter_x_min + 1;
    int inter_h = inter_y_max - inter_y_min + 1;
    if (inter_w <= 0 || inter_h <= 0) return 0.0f;
    
    int inter_area = inter_w * inter_h;
    int box_a_area = (a.x_max - a.x_min + 1) * (a.y_max - a.y_min + 1);
    int box_b_area = (b.x_max - b.x_min + 1) * (b.y_max - b.y_min + 1);

    return (float)inter_area / (float)(box_a_area + box_b_area - inter_area);
}

static void swap_box(DetectionBox* a, DetectionBox* b) {
    DetectionBox temp = *a;
    *a = *b;
    *b = temp;
}

static int compare_boxes(const void* a, const void* b) {
    DetectionBox* box_a = (DetectionBox*)a;
    DetectionBox* box_b = (DetectionBox*)b;
    if (box_a->score > box_b->score) return -1;
    if (box_a->score < box_b->score) return 1;
    return 0;
}

static int nms(DetectionBox* boxes, int num_boxes, float iou_threshold, 
               DetectionBox* out_boxes, int max_out_boxes) {
    if (num_boxes == 0) return 0;
    
    qsort(boxes, num_boxes, sizeof(DetectionBox), compare_boxes);

    int selected[128];
    int count = 0;

    for (int i = 0; i < num_boxes && count < max_out_boxes; ++i) {
        int keep = 1;
        for (int j = 0; j < count; ++j) {
            if (iou(boxes[i], boxes[selected[j]]) > iou_threshold) {
                keep = 0;
                break;
            }
        }
        if (keep) {
            selected[count++] = i;
        }
    }

    for (int i = 0; i < count; ++i) {
        out_boxes[i] = boxes[selected[i]];
    }
    
    return count;
}

static int post_process_ssd_mobilenet(float* out_data, DetectionBox* boxes, int max_boxes) {
    float padw = 0.125f, padh = 0.0f;
    float max_shape = 640.0f;
    int num_boxes = 0;
    DetectionBox temp_boxes[128];

    for (int i = 0; i < 1344 && num_boxes < 128; i++) {
        float* conf = out_data + 1344 * 4;
        if (conf[i] < NMS_CONFIDENCE_THRESHOLD)
            continue;
            
        float x = out_data[i] - padw;
        float y = out_data[i + 1344] - padh;
        float w = out_data[i + 1344 * 2];
        float h = out_data[i + 1344 * 3];

        int x_min = (int)((x - w / 2.0f) * max_shape);
        int x_max = (int)((x + w / 2.0f) * max_shape);
        int y_min = (int)((y - h / 2.0f) * max_shape);
        int y_max = (int)((y + h / 2.0f) * max_shape);

        temp_boxes[num_boxes].x_min = x_min;
        temp_boxes[num_boxes].x_max = x_max;
        temp_boxes[num_boxes].y_min = y_min;
        temp_boxes[num_boxes].y_max = y_max;
        temp_boxes[num_boxes].score = conf[i];
        num_boxes++;
    }

    return nms(temp_boxes, num_boxes, 0.4f, boxes, max_boxes);
}

extern "C" bool ai_init(int mode)
{
    xSemaphoreTake(g_ai_mutex, portMAX_DELAY);
    LOG_INFO("AI Engine init: mode=%d", mode);
    
    if (mode == MODE_LEVEL1) {
        model = tflite::GetModel(___pretrained_models_coco_2017_person_ssd_mobilenet_v2_fpnlite_035_224_ssd_mobilenet_v2_fpnlite_035_224_int8_tflite);
        if (model->version() != TFLITE_SCHEMA_VERSION) {
            LOG_ERROR("Model schema version mismatch: %d != %d", 
                     model->version(), TFLITE_SCHEMA_VERSION);
            xSemaphoreGive(g_ai_mutex);
            return false;
        }

        // 设置 OpResolver
        static tflite::MicroMutableOpResolver<15> op_resolver;
        op_resolver.AddQuantize();
        op_resolver.AddConv2D();
        op_resolver.AddDepthwiseConv2D();
        op_resolver.AddPad();
        op_resolver.AddAdd();
        op_resolver.AddShape();
        op_resolver.AddStridedSlice();
        op_resolver.AddPack();
        op_resolver.AddReshape();
        op_resolver.AddResizeBilinear();
        op_resolver.AddConcatenation();
        op_resolver.AddSoftmax();
        op_resolver.AddDequantize();

        // 构建解释器
        static tflite::MicroInterpreter static_interpreter(
            model, op_resolver, tensor_arena, kTensorArenaSize);
        interpreter = &static_interpreter;

        if (interpreter->AllocateTensors() != kTfLiteOk) {
            LOG_ERROR("AllocateTensors() failed");
            xSemaphoreGive(g_ai_mutex);
            return false;
        }

        input = interpreter->input(0);
        output = interpreter->output(0);

        if (input == nullptr || output == nullptr) {
            LOG_ERROR("Input or output tensor is null");
            xSemaphoreGive(g_ai_mutex);
            return false;
        }
        
        LOG_INFO("TFLM initialized successfully");
        LOG_INFO("Input shape: %dx%dx%d", input->dims->data[1], input->dims->data[2], input->dims->data[3]);
    }
    
    s_current_mode = mode;
    s_ai_initialized = true;
    xSemaphoreGive(g_ai_mutex);
    return true;
}

extern "C" bool ai_switch_mode(int new_mode)
{
    xSemaphoreTake(g_ai_mutex, portMAX_DELAY);
    LOG_INFO("Switching AI mode: %d -> %d", s_current_mode, new_mode);
    s_current_mode = new_mode;
    xSemaphoreGive(g_ai_mutex);
    return true;
}

extern "C" bool ai_run_inference(uint8_t *input_frame)
{
    xSemaphoreTake(g_ai_mutex, portMAX_DELAY);
    if (!s_ai_initialized || interpreter == nullptr) {
        xSemaphoreGive(g_ai_mutex);
        return false;
    }
    
    if (s_current_mode == MODE_LEVEL1) {
        if (input != nullptr && input_frame != nullptr) {
            memcpy(input->data.uint8, input_frame, 
                   LEVEL1_INPUT_WIDTH * LEVEL1_INPUT_HEIGHT * LEVEL1_INPUT_CHANNELS);
        }
        
        if (interpreter->Invoke() != kTfLiteOk) {
            LOG_ERROR("Invoke failed");
            xSemaphoreGive(g_ai_mutex);
            return false;
        }
        
        float* out_data = output->data.f;
        s_num_detected_boxes = post_process_ssd_mobilenet(out_data, s_detected_boxes, 10);
        
        if (s_num_detected_boxes > 0) {
            s_last_result = 1;
            s_last_confidence = s_detected_boxes[0].score;
        } else {
            s_last_result = 0;
            s_last_confidence = 0.0f;
        }
        
        xSemaphoreGive(g_ai_mutex);
        return true;
    }
    
    s_last_result = 0;
    s_last_confidence = 0.0f;
    xSemaphoreGive(g_ai_mutex);
    return true;
}

extern "C" int ai_get_detection_result(void)
{
    xSemaphoreTake(g_ai_mutex, portMAX_DELAY);
    int result = s_last_result;
    xSemaphoreGive(g_ai_mutex);
    return result;
}

extern "C" float ai_get_confidence(void)
{
    xSemaphoreTake(g_ai_mutex, portMAX_DELAY);
    float conf = s_last_confidence;
    xSemaphoreGive(g_ai_mutex);
    return conf;
}

extern "C" int ai_get_detection_boxes(DetectionBox* boxes, int max_boxes)
{
    xSemaphoreTake(g_ai_mutex, portMAX_DELAY);
    if (boxes == nullptr || max_boxes <= 0) {
        xSemaphoreGive(g_ai_mutex);
        return 0;
    }
    
    int count = (s_num_detected_boxes < max_boxes) ? s_num_detected_boxes : max_boxes;
    for (int i = 0; i < count; i++) {
        boxes[i] = s_detected_boxes[i];
    }
    
    xSemaphoreGive(g_ai_mutex);
    return count;
}
