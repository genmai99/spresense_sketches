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

#define RECORD_FILE_NAME "data.txt"
#define ANALOG_MIC_GAIN  210 /* Range:-785(-78.5dB) to 210(+21.0dB) */

SDClass theSD;
MediaRecorder *theRecorder;

/* Define the filename */
File myFile;

/*
 * Sample's buffer_size: 768sample/frame*16bit(2Byte)*4ch = 6144
 * MySetting: 768sample/frame*16bit(2Byte)*1ch = 1536
 */
static const int32_t recoding_frames = 400;
static const int32_t buffer_size = 6144; 
static uint8_t       s_buffer[buffer_size];

/* Time in ms unit */
unsigned long ms_time=0;

/* Data in the string mode */
String dataString = "";

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
 * Recording bit rate
 * Set in bps.
 * Bitrate is effective only when mp3 recording
 */
static const uint32_t recoding_bitrate = 96000;

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
  theRecorder->init(AS_CODECTYPE_LPCM,
                    recoding_cannel_number,
                    recoding_sampling_rate,
                    recoding_bit_length,
                    recoding_bitrate, /* Bitrate is effective only when mp3 recording */
                    "/mnt/sd0/BIN");


  /* Open file for data write on SD card */
  while (!theSD.begin()) {
    ; /* wait until SD card is mounted. */
  }
  
  if (theSD.exists(RECORD_FILE_NAME))
    {
      printf("Remove existing file [%s].\n", RECORD_FILE_NAME);
      theSD.remove(RECORD_FILE_NAME);
    }
  
  /* Create a new directory */
  theSD.mkdir("dir/");

  myFile = theSD.open(RECORD_FILE_NAME, FILE_WRITE);

  /* Verify file open */   
  if (!myFile)
    {
      printf("File open error\n");
      exit(1);
    }

  printf("Open! [%s]\n", RECORD_FILE_NAME);

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
  printf("Size %d [%02x %02x %02x %02x %02x %02x %02x %02x ...]\n",
         size,
         s_buffer[0],
         s_buffer[1],
         s_buffer[2],
         s_buffer[3],
         s_buffer[4],
         s_buffer[5],
         s_buffer[6],
         s_buffer[7]);
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
  dataString += String(ms_time,DEC);

  /* make a acc data to send in string mode */
  for(int i=0; i<3; i++){
    dataString += ","; 
    dataString += String(acc[i],DEC);
  }

  /* make a gyro data to send in string mode */
  for(int i=0; i<3; i++){
    dataString += ","; 
    dataString += String(gyro[i],DEC);
  }
  
  dataString += "\n";

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
      
      /* make a audio data to send in string mode */
      /*
      for(int i=0; i<8; i++){
        dataString += ","; 
        dataString=String(s_buffer[i],DEC);
        }*/
      
      //printf("ax:%.2f, ay:%.2f, az:%.2f, gx:%.2f, gy:%.2f, gz:%.2f\n", ax, ay, az, gx, gy, gz);

    }

  /* This sleep is adjusted by the time to write the audio stream file.
     Please adjust in according with the processing contents
     being processed at the same time by Application.
  */
  
//  usleep(10000);

  /* Stop Recording */
  if (total_size > (recoding_frames*buffer_size))
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

  /* Save data to SD card*/
  //int ret = myFile.write((uint8_t*)&s_buffer, read_size);
  int ret = myFile.println(dataString);
  
  if (ret < 0){
    puts("File write error.");
    err = MEDIARECORDER_ECODE_FILEACCESS_ERROR;
  }

  myFile.close();

  theRecorder->deactivate();
  theRecorder->end();
  
  puts("End Recording");
  exit(1);

}
