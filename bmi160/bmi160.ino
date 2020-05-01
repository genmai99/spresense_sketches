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
 
#include <BMI160Gen.h>
#include <MemoryUtil.h>
#include <MadgwickAHRS.h>

Madgwick MadgwickFilter;

/* Time in ms unit */
unsigned long ms_time=0;

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

  printf("roll:%f, pitch:%f, yaw:%f\n", rpy[0], rpy[1], rpy[2]);

}
