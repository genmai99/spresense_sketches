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
#include <MemoryUtil.h>
#include <MadgwickAHRS.h>

#define SENSOR_DIRECTORY_NAME "bmi160/"

SDClass theSD;
Madgwick MadgwickFilter;

/* Define the filename */
File sensorFile;

/* Time in ms unit */
unsigned long ms_time=0;

/* Data in the string mode */
String sensor_file_path = SENSOR_DIRECTORY_NAME; /* Data string for save directory */
String sensor_data = "time, ax, ay, az, gx, gy, gz, roll[deg/s], pitch[deg/s], yaw[deg/s]\n"; /* Data string for acc and gyro*/

int loop_count=0;
int savefile_no = 0;

/**
 *  @brief Setup BMI160 device to measure acc and gyro
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

  /* Set MadgwickFilter */
  MadgwickFilter.begin(100); //100Hz

  puts("Initializing IMU device...done.");
  
  /* Open file for data write on SD card */
  while (!theSD.begin()) {
    ; /* wait until SD card is mounted. */
  }
  
  /* Create a new directory */
  theSD.mkdir(SENSOR_DIRECTORY_NAME);
  
  /* File open */
  sensorFile = theSD.open(sensor_file_path, FILE_WRITE);

  /* Verify file open */   
  if (!sensorFile){
    printf("Sensor File open error\n");
    exit(1);
  }
  
  printf("Open! [%s]\n", SENSOR_FILE_NAME);  
}


/**
 * @brief Measure acc and gyro of PCM data into buffer
 */
void loop() {

  /* For BMI160 */
  float acc[3];   //scaled accelerometer values
  float gyro[3];  //scaled Gyro values
  float rpy[3];   //scaled Gyro values

  /* For Analog mic */
  static int32_t total_size = 0;
  uint32_t read_size = 0;

  /* time (msec) after the program was initiated */
  ms_time=millis();

  /* read accelerometer and gyro measurements from device, scaled to the configured range */
  BMI160.readAccelerometerScaled(acc[0], acc[1], acc[2]);
  BMI160.readGyroScaled(gyro[0], gyro[1], gyro[2]);
  MadgwickFilter.updateIMU(gyro[0], gyro[1], gyro[2], acc[0], acc[1], acc[2]);
  rpy[0] = MadgwickFilter.getRoll();
  rpy[1] = MadgwickFilter.getPitch();
  rpy[2] = MadgwickFilter.getYaw();

  
  /* make a time data to send in string mode */
  sensor_data += String(ms_time,DEC);

  /* make a acc data to send in string mode */
  for(int i=0; i<3; i++){
    sensor_data += ","; 
    sensor_data += String(acc[i],DEC);
  }

  for(int i=0; i<3; i++){
    sensor_data += ","; 
    sensor_data += String(gyro[i],DEC);
  }

  /* make a gyro data to send in string mode */
  for(int i=0; i<3; i++){
    sensor_data += ","; 
    sensor_data += String(rpy[i],DEC);
  }
  
  sensor_data += "\n";

}
