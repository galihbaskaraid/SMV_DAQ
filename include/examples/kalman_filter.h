// ============================================================================
// EXAMPLE APPLICATION: KALMAN FILTER FOR IMU DATA
// ============================================================================
// Implements a simple Kalman filter for better IMU data quality

#ifndef EXAMPLE_KALMAN_FILTER_H
#define EXAMPLE_KALMAN_FILTER_H

#include <Arduino.h>

class KalmanFilter {
private:
    float q;  // Process noise
    float r;  // Measurement noise
    float x;  // Estimated state
    float p;  // Estimation error
    float k;  // Kalman gain
    
public:
    KalmanFilter(float process_noise = 0.01f, float measurement_noise = 0.1f)
        : q(process_noise), r(measurement_noise), x(0), p(1), k(0) {}
    
    float update(float measurement) {
        // Predict
        p = p + q;
        
        // Update
        k = p / (p + r);
        x = x + k * (measurement - x);
        p = (1 - k) * p;
        
        return x;
    }
    
    void setNoiseParams(float process_noise, float measurement_noise) {
        q = process_noise;
        r = measurement_noise;
    }
    
    void reset(float initial_value = 0) {
        x = initial_value;
        p = 1;
        k = 0;
    }
};

// 6-DOF IMU Kalman Filter
class IMUKalmanFilter {
private:
    KalmanFilter accel_x, accel_y, accel_z;
    KalmanFilter gyro_x, gyro_y, gyro_z;
    KalmanFilter temp;
    
public:
    IMUKalmanFilter(float q = 0.01f, float r = 0.1f)
        : accel_x(q, r), accel_y(q, r), accel_z(q, r),
          gyro_x(q, r), gyro_y(q, r), gyro_z(q, r),
          temp(q * 0.1f, r * 10.0f) {}
    
    void update(Vector3_t &accel, Vector3_t &gyro, float &temperature) {
        accel.x = accel_x.update(accel.x);
        accel.y = accel_y.update(accel.y);
        accel.z = accel_z.update(accel.z);
        
        gyro.x = gyro_x.update(gyro.x);
        gyro.y = gyro_y.update(gyro.y);
        gyro.z = gyro_z.update(gyro.z);
        
        temperature = temp.update(temperature);
    }
};

#endif // EXAMPLE_KALMAN_FILTER_H

// ============================================================================
// USAGE:
// ============================================================================

/*

#include "examples/kalman_filter.h"

IMUKalmanFilter imu_filter(0.01f, 0.1f);

// In sensor update:
void updateIMUWithFilter() {
    MPU6500Data_t data = g_data_sensor.mpu6500;
    imu_filter.update(data.accel, data.gyro, data.temperature);
    
    if (xSemaphoreTake(g_data_sensor_mutex, pdMS_TO_TICKS(100))) {
        g_data_sensor.mpu6500 = data;
        xSemaphoreGive(g_data_sensor_mutex);
    }
}

*/
