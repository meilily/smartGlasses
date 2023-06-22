/* Edge Impulse ingestion SDK
 * Copyright (c) 2022 EdgeImpulse Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 */

// If your target is limited in memory remove this macro to save 10K RAM
#define EIDSP_QUANTIZE_FILTERBANK   0


/**
 * Define the number of slices per model window. E.g. a model window of 1000 ms
 * with slices per model window set to 4. Results in a slice size of 250 ms.
 * For more info: https://docs.edgeimpulse.com/docs/continuous-audio-sampling
 */
#define EI_CLASSIFIER_SLICES_PER_MODEL_WINDOW 2

/*
 ** NOTE: If you run into TFLite arena allocation issue.
 **
 ** This may be due to may dynamic memory fragmentation.
 ** Try defining "-DEI_CLASSIFIER_ALLOCATION_STATIC" in boards.local.txt (create
 ** if it doesn't exist) and copy this file to
 ** `<ARDUINO_CORE_INSTALL_PATH>/arduino/hardware/<mbed_core>/<core_version>/`.
 **
 ** See
 ** (https://support.arduino.cc/hc/en-us/articles/360012076960-Where-are-the-installed-cores-located-)
 ** to find where Arduino installs cores on your machine.
 **
 ** If the problem persists then there's not enough memory for this model and application.
 */

/* Includes ---------------------------------------------------------------- */
#include <PDM.h>
#include <speechrecexpanded_inferencing.h> // should be replaced with library produced from edge impulse
#include <ArduinoBLE.h>
#include "er_oled.h" // OLED library ( some customizations made)
#include <Wire.h>
#define serverUUID        "---" //https://www.uuidgenerator.net/ -> use this to create custom uuid 
uint8_t oled_buf[WIDTH * HEIGHT / 8];
/** Audio buffers, pointers and selectors */
typedef struct {
    signed short *buffers[2];
    unsigned char buf_select;
    unsigned char buf_ready;
    unsigned int buf_count;
    unsigned int n_samples;
} inference_t;

static inference_t inference;
static bool record_ready = false;
static signed short *sampleBuffer;
static bool debug_nn = false; // Set this to true to see e.g. features generated from the raw signal
static int print_results = -(EI_CLASSIFIER_SLICES_PER_MODEL_WINDOW);


String condition="Clear"; // default condition
BLEService weatherRec("---b");  //https://www.uuidgenerator.net/ -> use this to create custom uuid 

//scattered clouds string is to fill the initial value 
BLECharacteristic storeStuff("---", BLERead | BLEWrite,"scattered clouds 123");  //https://www.uuidgenerator.net/ -> use this to create custom uuid 
byte array[25];

//default vals
String temp = "79"; 
String timeDisp = "3:45"; 
int h = 3;
int mi = 45;
boolean gotTime = false;
//end defaults

// takes care of getting setting up ble server so that xiaos3 connects and sends weather data
void bleStuff(){
   BLEDevice central = BLE.central();
    unsigned long tim = millis();
  
  if (central) {
    Serial.print("Connected to central: ");
    // print the central's MAC address:
    Serial.println(central.address());

    // while the central is still connected to peripheral:
    while (central.connected()) {
      // if the remote device wrote to the characteristic,
      if (storeStuff.written()) {
        if (storeStuff.value()) {   
          unsigned int len = storeStuff.valueSize();
           byte a[len];
           Serial.println(len);
           storeStuff.readValue(a,len);
           String total ="";
           condition = "";
           temp = "";
           String totTime = "";
           boolean conDone = false;
           int digNum = 0;
           for(unsigned i =0; i < len;i++){
             char n = a[i];
             total+= n;
             Serial.println(n);
           } 
           for(unsigned i = 0; i < total.length();i++){
             if(isAlpha(total[i]) && !conDone){
                condition += total[i];
             }
             else if(isDigit(total[i])){
                if(digNum < 2){
                  temp+= total[i];
                  digNum += 1;
                  Serial.println(digNum);
                }
                else if(digNum >= 2){
                  totTime += total[i];
                }
                conDone = true;
             }
             else if(total[i]=='.'){
               break;
             }
           }
           Serial.println(condition);
           Serial.println(temp);
           Serial.println(totTime);
           if(!gotTime){
             h = (totTime[0]-48) * 10;
             h+= totTime[1] - 48;
             mi = (totTime[2]-48) * 10;
             mi+= totTime[3] - 48;
             String hou = String(h);
             String minut = String(mi);
                if(h  < 10 ){
             hou = "0" + String(h);
             }
             if(mi < 10 ){
                minut = "0" + String(mi);
             }
             timeDisp = hou + ":" + minut;
             gotTime = true;
           }
           Serial.println(totTime);
           Serial.println("STUFF HAS BEEN WRITTEN"); 
           central.disconnect();   
        }
      }
    }


    Serial.print("We have disconnected");
    Serial.println(central.address());
  }
}

// used to draw weather symbol depending on info 
void drawWeather(){
    uint8_t in = 48;

    if(condition == "Clouds"){
      er_oled_bitmap(in,0,cloudLeft,8,16,oled_buf);
      er_oled_bitmap(in+8,0,cloudRight,8,16,oled_buf);
    }
    else if(condition == "Fog"){
      er_oled_bitmap(in,0,fogLeft,8,16,oled_buf);
      er_oled_bitmap(in+8,0,fogRight,8,16,oled_buf);
    }
    else if(condition == "Clear"){
      er_oled_bitmap(in,0,topSun,8,16,oled_buf);
      er_oled_bitmap(in+8,0,bottomSun,8,16,oled_buf);
    }
    else if(condition == "Snow"){
      er_oled_bitmap(in,0,topSnow,8,16,oled_buf);
      er_oled_bitmap(in+8,0,bottomSnow,8,16,oled_buf);
    }
    else if(condition == "Rain"){
      er_oled_bitmap(in,0,rainLeft,8,16,oled_buf);
      er_oled_bitmap(in+8,0,rainRight,8,16,oled_buf);
    }
    else{
      er_oled_bitmap(in,0,topSun,8,16,oled_buf);
      er_oled_bitmap(in+8,0,bottomSun,8,16,oled_buf);
    }
}

boolean timeUp = false;
int timeM = 0;
int timeS = 0;
long timerTime = 0;
enum curScreen{HOME,WEATHER,TIMER} currentScreen; // different screens that can be selected

//timer logic
String setTimer(){
  if(millis() - timerTime >= 1000){
      timerTime = millis();
        if(timeS < 59){
          timeS++;
        }
        else{
          timeS = 0;
          if(timeM < 59){
            timeM++;
          }
          else{
            timeM = 0;
          }
        }      
    }
        String tsec = String(timeS);
        String tmi = String(timeM);
        if(timeM  < 10 ){
          tmi = "0" + String(timeM);
        }
        if(timeS < 10 ){
          tsec = "0" + String(timeS);
        }
        return tmi + ":" + tsec;
}
String printTimer(){
   String tsec = String(timeS);
        String tmi = String(timeM);
        if(timeM  < 10 ){
          tmi = "0" + String(timeM);
        }
        if(timeS < 10 ){
          tsec = "0" + String(timeS);
        }
        return tmi + ":" + tsec;
}

//screen logic
void runScreen(){
  String in = "TEMP:" + temp + "F";
  String timerTime = "00:00";
  if(timeUp){
    Serial.println("increasing");
    timerTime = setTimer();
  }
  else{
    Serial.println("noChange");
    timerTime = printTimer();
  }
 
  switch(currentScreen){
    case HOME:
      er_oled_clear(oled_buf);
      customString(20,0,timeDisp.c_str(),1,oled_buf);
      er_oled_display(oled_buf);
    break;
    case WEATHER:
      er_oled_clear(oled_buf);
      Serial.println(in);
      customString(0,0,in.c_str(),1,oled_buf);
      drawWeather();
      er_oled_display(oled_buf); 
    break;
    case TIMER:
      er_oled_clear(oled_buf);
      customString(28,0,timerTime.c_str(),1,oled_buf);
      er_oled_display(oled_buf);
    break;
    default:
    break;
  }
}

//startup for screen
void screenSet(){
 // Serial.begin(9600);
  Wire.begin();
  
  /* display an image of bitmap matrix */
  er_oled_begin();
  command(0xa6);//--set normal display  
  
}

//MCU startup 
void setup()
{
   currentScreen = HOME;
    
    Serial.begin(9600);
    screenSet();
    if (!BLE.begin()) {
    Serial.println("BLE fail");
    }
    BLE.setLocalName("weather");
    BLE.setAdvertisedService(weatherRec);

  // add the characteristic to the service
    weatherRec.addCharacteristic(storeStuff);

  
   BLE.addService(weatherRec);

   BLE.advertise();

    // summary of inferencing settings (from model_metadata.h)
    ei_printf("Inferencing settings:\n");
    ei_printf("\tInterval: %.2f ms.\n", (float)EI_CLASSIFIER_INTERVAL_MS);
    ei_printf("\tFrame size: %d\n", EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE);
    ei_printf("\tSample length: %d ms.\n", EI_CLASSIFIER_RAW_SAMPLE_COUNT / 16);
    ei_printf("\tNo. of classes: %d\n", sizeof(ei_classifier_inferencing_categories) /
                                            sizeof(ei_classifier_inferencing_categories[0]));

    run_classifier_init();
    if (microphone_inference_start(EI_CLASSIFIER_SLICE_SIZE) == false) {
        ei_printf("ERR: Could not allocate audio buffer (size %d), this could be due to the window length of your model\r\n", EI_CLASSIFIER_RAW_SAMPLE_COUNT);
        return;
    }
}


// main mcu loop code
unsigned long myTime = 0;
void loop()
{
    Serial.print(timeS);
     if(millis() - myTime >= 60000){
      myTime = millis();
        if(mi < 59){
          mi++;
        }
        else{
          mi = 0;
          if(h < 23){
            h++;
          }
          else{
            h = 0;
          }
        }
        String hou = String(h);
        String minut = String(mi);
        if(h  < 10 ){
          hou = "0" + String(h);
        }
        if(mi < 10 ){
          minut = "0" + String(mi);
        }
        timeDisp = hou + ":" + minut;
    }
    bool m = microphone_inference_record();
    if (!m) {
        ei_printf("ERR: Failed to record audio...\n");
        return;
    }

    signal_t signal;
    signal.total_length = EI_CLASSIFIER_SLICE_SIZE;
    signal.get_data = &microphone_audio_signal_get_data;
    ei_impulse_result_t result = {0};

    EI_IMPULSE_ERROR r = run_classifier_continuous(&signal, &result, debug_nn);
    if (r != EI_IMPULSE_OK) {
        ei_printf("ERR: Failed to run classifier (%d)\n", r);
        return;
    }
    // screen and other essential functions
     bleStuff();
    size_t ind = 1;
    for(size_t o = 2; o < EI_CLASSIFIER_LABEL_COUNT; o++){
      if(result.classification[o].value>0.7){
          ind = o;
      }
    }
    if(result.classification[ind].value>0.7 && ind !=1){
        if(ind == 2){
        currentScreen = HOME;
        }
        else if(ind == 4){
        currentScreen = TIMER;
        }
        else if(ind == 5){
        currentScreen = WEATHER;
        }
        else if(ind == 3 && currentScreen == TIMER){
          if(!timeUp){
            timeM = 0;
            timeS = 0;
          }
          timeUp = false;
        }
        else if(ind == 6 && currentScreen == TIMER){
          timeUp = true;
        }
    }
    runScreen();
    //main functions end

    if (++print_results >= (EI_CLASSIFIER_SLICES_PER_MODEL_WINDOW)) {
        // print the predictions
        ei_printf("Predictions ");
        ei_printf("(DSP: %d ms., Classification: %d ms., Anomaly: %d ms.)",
            result.timing.dsp, result.timing.classification, result.timing.anomaly);
        ei_printf(": \n");
        for (size_t ix = 0; ix < EI_CLASSIFIER_LABEL_COUNT; ix++) {
            ei_printf("    %s: %.5f\n", result.classification[ix].label,
                      result.classification[ix].value);
        }
#if EI_CLASSIFIER_HAS_ANOMALY == 1
        ei_printf("    anomaly score: %.3f\n", result.anomaly);
#endif

        print_results = 0;
    }
}

/**
 * @brief      PDM buffer full callback
 *             Get data and call audio thread callback
 */
static void pdm_data_ready_inference_callback(void)
{
    int bytesAvailable = PDM.available();

    // read into the sample buffer
    int bytesRead = PDM.read((char *)&sampleBuffer[0], bytesAvailable);

    if (record_ready == true) {
        for (int i = 0; i<bytesRead>> 1; i++) {
            inference.buffers[inference.buf_select][inference.buf_count++] = sampleBuffer[i];

            if (inference.buf_count >= inference.n_samples) {
                inference.buf_select ^= 1;
                inference.buf_count = 0;
                inference.buf_ready = 1;
            }
        }
    }
}

/**
 * @brief      Init inferencing struct and setup/start PDM
 *
 * @param[in]  n_samples  The n samples
 *
 * @return     { description_of_the_return_value }
 */
static bool microphone_inference_start(uint32_t n_samples)
{
    inference.buffers[0] = (signed short *)malloc(n_samples * sizeof(signed short));

    if (inference.buffers[0] == NULL) {
        return false;
    }

    inference.buffers[1] = (signed short *)malloc(n_samples * sizeof(signed short));

    if (inference.buffers[1] == NULL) {
        free(inference.buffers[0]);
        return false;
    }

    sampleBuffer = (signed short *)malloc((n_samples >> 1) * sizeof(signed short));

    if (sampleBuffer == NULL) {
        free(inference.buffers[0]);
        free(inference.buffers[1]);
        return false;
    }

    inference.buf_select = 0;
    inference.buf_count = 0;
    inference.n_samples = n_samples;
    inference.buf_ready = 0;

    // configure the data receive callback
    PDM.onReceive(&pdm_data_ready_inference_callback);

    PDM.setBufferSize((n_samples >> 1) * sizeof(int16_t));

    // initialize PDM with:
    // - one channel (mono mode)
    // - a 16 kHz sample rate
    if (!PDM.begin(1, EI_CLASSIFIER_FREQUENCY)) {
        ei_printf("Failed to start PDM!");
    }

    // set the gain, defaults to 20
    PDM.setGain(127);

    record_ready = true;

    return true;
}

/**
 * @brief      Wait on new data
 *
 * @return     True when finished
 */
static bool microphone_inference_record(void)
{
    bool ret = true;

    if (inference.buf_ready == 1) {
        ei_printf(
            "Error sample buffer overrun. Decrease the number of slices per model window "
            "(EI_CLASSIFIER_SLICES_PER_MODEL_WINDOW)\n");
        ret = false;
    }

    while (inference.buf_ready == 0) {
        delay(1);
    }

    inference.buf_ready = 0;

    return ret;
}

/**
 * Get raw audio signal data
 */
static int microphone_audio_signal_get_data(size_t offset, size_t length, float *out_ptr)
{
    numpy::int16_to_float(&inference.buffers[inference.buf_select ^ 1][offset], out_ptr, length);

    return 0;
}

/**
 * @brief      Stop PDM and release buffers
 */
static void microphone_inference_end(void)
{
    PDM.end();
    free(inference.buffers[0]);
    free(inference.buffers[1]);
    free(sampleBuffer);
}

#if !defined(EI_CLASSIFIER_SENSOR) || EI_CLASSIFIER_SENSOR != EI_CLASSIFIER_SENSOR_MICROPHONE
#error "Invalid model for current sensor."
#endif
