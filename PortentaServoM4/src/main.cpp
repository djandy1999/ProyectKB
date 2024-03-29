#include "RPC.h"
#include <Arduino.h>

#include <TensorFlowLite.h>
#include <tensorflow/lite/micro/all_ops_resolver.h>
#include <tensorflow/lite/micro/micro_error_reporter.h>
#include <tensorflow/lite/micro/micro_interpreter.h>
#include <tensorflow/lite/schema/schema_generated.h>
#include <tensorflow/lite/version.h>

// Include the TensorFlow Lite model file.
#include "model.h"

static float e_values[8];
  
static char serial_in;

bool connBool = false;

// Create an area of memory to use for input, output, and intermediate arrays.
constexpr int kTensorArenaSize = 40 * 1024;
uint8_t tensor_arena[kTensorArenaSize];

// Define input array shape.
const int input_shape[] = {1, 2, 4, 1};

tflite::ErrorReporter* error_reporter;
tflite::MicroInterpreter* interpreter;

#define Serial RPC  // Create alias


void setup() {
  Serial.begin();   // RPC.begin does not take a baud rate

  // Set up logging.
  static tflite::MicroErrorReporter micro_error_reporter;
  tflite::ErrorReporter* error_reporter = &micro_error_reporter;

  // Map the model into a usable data structure.
  static tflite::AllOpsResolver resolver;
  const tflite::Model* model = tflite::GetModel(model_final_v1_3_tflite);
  static tflite::MicroInterpreter static_interpreter(model, resolver, tensor_arena, kTensorArenaSize, error_reporter);
  interpreter = &static_interpreter;
  interpreter->AllocateTensors();

  // Get information about the memory area to use for the model's input.
  //TfLiteTensor* input = interpreter->input(0);
}

void loop(){

  if(Serial.available()>0){
    serial_in =  Serial.read();
    
    switch (serial_in){
      case 'c':
            Serial.println("MFour Succesfully Conected");
            connBool = true;
            break;
      case 'e':
        for(int i = 0; i < 8;i++){
          // Read 8 sensor values into an array.
          e_values[i] = Serial.parseFloat();
          
        }
        
        }
        if(connBool){ 
          
              
          TfLiteTensor* input = interpreter->input(0);
        // Reshape the input data according to the model's input shape (1, 2, 4, 1).
        for (int i = 0; i < input_shape[1]; i++) {
          for (int j = 0; j < input_shape[2]; j++) {
            input->data.f[i * input_shape[2] + j] = e_values[i * input_shape[2] + j];
          }
        }

        // // Print the reshaped input data.
        // Serial.println("Input data:");
        //  for (int i = 0; i < input_shape[1]; i++) {
        //    for (int j = 0; j < input_shape[2]; j++) {
        //      Serial.print(input->data.f[i * input_shape[2] + j], 6);
        //      Serial.print(" ");
        //    }
        //    Serial.println();
        //  }

          // Invoke the TensorFlow Lite model.
          TfLiteStatus invoke_status = interpreter->Invoke();
          if (invoke_status != kTfLiteOk) {
            Serial.println("Invoke failed.");
            return;
          }
          

          // Get the output tensor from the model.
          TfLiteTensor* output = interpreter->output(0);

          // Find the predicted gesture index with the highest probability.
          int predicted_gesture = 0;
          float max_probability = output->data.f[0];
          for (int i = 1; i < output->dims->data[1]; i++) {
            
            if (output->data.f[i] > max_probability) {
              max_probability = output->data.f[i];
              predicted_gesture = i;
            }
          }

          // Print the predicted gesture index.
          Serial.println(predicted_gesture);

          // Wait before looping again.
          
    } 
    }

}


  



    
 