/*
 *  action_recognition_module.ino - PCM capture using object if, Accelerometer, Gyro example application
 *  Copyright 2018 Sony Semiconductor Solutions Corporation
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2.1 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */
 
#include <SDHCI.h>
#include <BMI160Gen.h>
#include <MediaRecorder.h>
#include <MemoryUtil.h>

#define SENSOR_FILE_NAME "sensor_data.txt"
#define RECORD_FILE_NAME "sound.wav"
#define SENSOR_DIRECTORY_NAME "result_sensor/"
#define RECORD_DIRECTORY_NAME "result_wav/"
#define ANALOG_MIC_GAIN 210 /* Range:-785(-78.5dB) to 210(+21.0dB) */

SDClass theSD;
MediaRecorder *theRecorder;

/* Define the filename */
File sensorFile;
File s_myFile;

/* Time in ms unit */
unsigned long ms_time=0;

/* Data in the string mode */
String sensor_file_path = SENSOR_DIRECTORY_NAME; /* Data string for save directory */
String record_file_path = RECORD_DIRECTORY_NAME; /* Data string for save directory */
String sensor_data = "time, ax, ay, az, gx, gy, gz\n"; /* Data string for acc and gyro*/

bool ErrEnd = false;

/**
 * @brief Audio attention callback
 *
 * When audio internal error occurc, this function will be called back.
 */
void mediarecorder_attention_cb(const ErrorAttentionParam *atprm)
{
  puts("Attention!");
  
  if (atprm->error_code >= AS_ATTENTION_CODE_WARNING)
    {
      ErrEnd = true;
   }
}

/** 
 * Sampling rate
 * Set 16000 or 48000
 */
static const uint32_t recoding_sampling_rate = 48000;

/** 
 * Number of input channels
 * Set either 1, 2, or 4.
 */
static const uint8_t  recoding_cannel_number = 1;

/** 
 * Audio bit depth
 * Set 16 or 24
 */
static const uint8_t  recoding_bit_length = 16;

/* Recording time[second] */
static const uint32_t recoding_time = 60;

/* Bytes per second
 * ex) 48000[Hz] * 1[ch] * 2[Byte](16bit/8bit) = 96000[B/s]
 */
static const int32_t recoding_byte_per_second = recoding_sampling_rate *
                                                recoding_cannel_number *
                                                recoding_bit_length / 8;

/* Total recording size
 * ex) 96000[B/s] * 10[s] = 960000[B] = 960[kB]
 */
static const int32_t recoding_size = recoding_byte_per_second * recoding_time;

/* One frame size
 * Calculated with 768 samples per frame.
 * ex) 768[sample] * 1[ch] * 2Byte(16bit/8bit) = 1536
 */
static const uint32_t frame_size  = 768 * recoding_cannel_number * (recoding_bit_length / 8);

/* Buffer size
 * Align in 512byte units based on frame size.
 */

static const uint32_t buffer_size = (frame_size + 511) & ~511;
static uint8_t        s_buffer[buffer_size];

/**
 * @brief Recorder done callback procedure
 *
 * @param [in] event        AsRecorderEvent type indicator
 * @param [in] result       Result
 * @param [in] sub_result   Sub result
 *
 * @return true on success, false otherwise
 */
static bool mediarecorder_done_callback(AsRecorderEvent event, uint32_t result, uint32_t sub_result)
{
  printf("mp cb %x %x %x\n", event, result, sub_result);

  return true;
}

/**
 *  @brief Setup audio device to capture PCM stream
 *
 *  Select input device as microphone <br>
 *  Set PCM capture sapling rate parameters to 48 kb/s <br>
 *  Set channel number 4 to capture audio from 4 microphones simultaneously <br>
 *  System directory "/mnt/sd0/BIN" will be searched for PCM codec
 */
void setup()
{
  /* initialize IMU160 device */
  puts("Initializing IMU device...");
  BMI160.begin();
  uint8_t dev_id = BMI160.getDeviceID();

  /* Set the accelerometer range to 2G (or 4, 8, 16G) */
  BMI160.setAccelerometerRange(2);

  /* Set the accelerometer range to 250 degrees/second (or 125, 500, 1000, 2000) */
  BMI160.setGyroRange(250);

  puts("Initializing IMU device...done.");
  
  /* Initialize memory pools and message libs */
  initMemoryPools();
  createStaticPools(MEM_LAYOUT_RECORDER);

  /* start audio system */
  theRecorder = MediaRecorder::getInstance();

  theRecorder->begin(mediarecorder_attention_cb);

  puts("initialization MediaRecorder");

  /* Set capture clock */
  theRecorder->setCapturingClkMode(MEDIARECORDER_CAPCLK_NORMAL);

  /* Activate Objects. Set output device to Speakers/Headphones */
  theRecorder->activate(AS_SETRECDR_STS_INPUTDEVICE_MIC, mediarecorder_done_callback);

  usleep(100 * 1000); /* waiting for Mic startup */

  /*
   * Initialize recorder to decode stereo wav stream with 48kHz sample rate
   * Search for SRC filter in "/mnt/sd0/BIN" directory
   */
  theRecorder->init(AS_CODECTYPE_WAV,
                    recoding_cannel_number,
                    recoding_sampling_rate,
                    recoding_bit_length,
                    AS_BITRATE_8000, /* Bitrate is effective only when mp3 recording */
                    "/mnt/sd0/BIN");

  sensor_file_path += SENSOR_FILE_NAME;
  record_file_path += RECORD_FILE_NAME;

  /* Open file for data write on SD card */
  while (!theSD.begin()) {
    ; /* wait until SD card is mounted. */
  }

  if (theSD.exists(sensor_file_path)){
      printf("Remove existing file [%s].\n", SENSOR_FILE_NAME);
      theSD.remove(sensor_file_path);
  }
  if (theSD.exists(record_file_path)){
      printf("Remove existing file [%s].\n", RECORD_FILE_NAME);
      theSD.remove(record_file_path);
  }  
  
  /* Create a new directory */
  theSD.mkdir(SENSOR_DIRECTORY_NAME);
  theSD.mkdir(RECORD_DIRECTORY_NAME);

  sensorFile = theSD.open(sensor_file_path, FILE_WRITE);
  s_myFile = theSD.open(record_file_path, FILE_WRITE);

  /* Verify file open */   
  if (!sensorFile){
    printf("Sensor File open error\n");
    exit(1);
  }
  if (!s_myFile){
    printf("Wav File open error\n");
    exit(1);
  }
  
  printf("Open! [%s] and [%s]\n", SENSOR_FILE_NAME, RECORD_FILE_NAME);

  /* Write wav header (Write to top of file. File size is tentative.) */
  theRecorder->writeWavHeader(s_myFile);
  puts("Write Header!");
  
  /* Set Gain */ 
  theRecorder->setMicGain(ANALOG_MIC_GAIN);

  /* Start Recorder */  
  theRecorder->start();
  puts("Recording Start!");
  
}

/**
 * @brief Audio signal process (Modify for your application)
 */
void signal_process(uint32_t size)
{
  /* Put any signal process */ 
  printf("Size %d\n", size);
}

/**
 * @brief Execute frames for FIFO empty
 */
void execute_frames()
{
  uint32_t read_size = 0;
  do
    {
      err_t err = execute_aframe(&read_size);
      if ((err != MEDIARECORDER_ECODE_OK)
       && (err != MEDIARECORDER_ECODE_INSUFFICIENT_BUFFER_AREA))
        {
          break;
        }
    }
  while (read_size > 0);
}

/**
 * @brief Execute one frame
 */
err_t execute_aframe(uint32_t* size)
{
  err_t err = theRecorder->readFrames(s_buffer, buffer_size, size);

  if(((err == MEDIARECORDER_ECODE_OK) || (err == MEDIARECORDER_ECODE_INSUFFICIENT_BUFFER_AREA)) && (*size > 0)) 
    {
      signal_process(*size);
    }
  int ret = s_myFile.write((uint8_t*)&s_buffer, *size);
  if (ret < 0){
    puts("File write error.");
    err = MEDIARECORDER_ECODE_FILEACCESS_ERROR;
  }

  return err;
}

/**
 * @brief Capture frames of PCM data into buffer
 */
void loop() {

  /* For BMI160 */
  float acc[3];   //scaled accelerometer values
  float gyro[3];  //scaled Gyro values

  /* For Analog mic */
  static int32_t total_size = 0;
  uint32_t read_size = 0;

  /* time (msec) after the program was initiated */
  ms_time=millis();

  /* read accelerometer and gyro measurements from device, scaled to the configured range */
  BMI160.readAccelerometerScaled(acc[0], acc[1], acc[2]);
  BMI160.readGyroScaled(gyro[0], gyro[1], gyro[2]);
  
  /* make a time data to send in string mode */
  sensor_data += String(ms_time,DEC);

  /* make a acc data to send in string mode */
  for(int i=0; i<3; i++){
    sensor_data += ","; 
    sensor_data += String(acc[i],DEC);
  }

  /* make a gyro data to send in string mode */
  for(int i=0; i<3; i++){
    sensor_data += ","; 
    sensor_data += String(gyro[i],DEC);
  }
  
  sensor_data += "\n";

  /* Execute audio data */
  err_t err = execute_aframe(&read_size);
  if (err != MEDIARECORDER_ECODE_OK && err != MEDIARECORDER_ECODE_INSUFFICIENT_BUFFER_AREA)
    {
      puts("Recording Error!");
      theRecorder->stop();
      goto exitRecording;      
    }
  else if (read_size>0)
    {
      total_size += read_size;
    }

  /* This sleep is adjusted by the time to write the audio stream file.
     Please adjust in according with the processing contents
     being processed at the same time by Application.
  */
  
  //usleep(10000);

  /* Stop Recording */
  if (total_size > recoding_size)
    {
      theRecorder->stop();

      /* Get ramaining data(flushing) */
      sleep(1); /* For data pipline stop */
      execute_frames();
      
      goto exitRecording;
    }

  if (ErrEnd)
    {
      printf("Error End\n");
      theRecorder->stop();
      goto exitRecording;
    }

  return;

exitRecording:

  theRecorder->writeWavHeader(s_myFile);
  puts("Update Header!");

  /* Save data to SD card*/
  int ret = sensorFile.println(sensor_data);
   
  if (ret < 0){
    puts("Sensor file write error.");
    err = MEDIARECORDER_ECODE_FILEACCESS_ERROR;
  }

  sensorFile.close();
  s_myFile.close();

  theRecorder->deactivate();
  theRecorder->end();
  
  puts("End Recording");
  exit(1);

}
