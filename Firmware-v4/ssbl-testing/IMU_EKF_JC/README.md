# IMU_EKF

This is a C++ implementation of an Error State Kalman Filter or Multiplicative Kalman Filter roughly based on [Attitude Error Representations for Kalman Filtering](https://www.researchgate.net/publication/245432681_Attitude_Error_Representations_for_Kalman_Filtering) by Landis Markley suitable for platform.io projects to fuse IMU measurements (gyroscope, accelerometer and magnetometer). In contrast to the paper I extended the state to x,y,z position, x,y,z velocities and x,y,z accelerations. Be aware that the linear velocity and position estimates are of no use. I included them just as an experiment. Instead of the gyro drift I added the angular velocity to the state. Also I use the quaternion to rotation matrix transformation from [Euclideanspace](https://www.euclideanspace.com/maths/geometry/rotations/conversions/quaternionToMatrix/index.htm).

## Dependencies

This implementation depends on Eigen. For platform.io projects you can use [Eigen_Platformio_Header](https://github.com/hobbeshunter/Eigen_Platformio_Header).

## Usage

```C++
#include <Eigen.h>
#include <ESKF.h>
#include <Quaternion.h>

// magnetometer calibration
Eigen::Matrix<float, 3, 3> W; // soft-iron
Eigen::Matrix<float, 3, 1> V; // hard-iron
float incl; // inclination
float B; // geomagnetic field strength

IMU_EKF::ESKF<float> filter;

void setup()
{
    // fill magnetometer calibration stuff
    // Winv << 
    // W << 
    // B = 
    // incl = 

    float ax, ay, az;
    // read accelerometer
    filter.initWithAcc(ax, ay, az);
}

void loop()
{
    float dt;
    bool readMag; // assume magnetometer has lower update rate as acellerometer and gyroscope

    // calculate dt

    // read sensors

    filter.predict(dt);
    filter.correctGyr(gx, gy, gz);
    filter.correctAcc(ax, ay, az);
    if (readMag)
    {
        filter.correctMag(mx, my, mz, incl, B, W, V);
    }
    filter.reset();

    // get attitude as roll, pitch, yaw
    float roll, pitch, yaw;
    filter.getAttitude(roll, pitch, yaw);
    // or quaternion
    IMU_EKF::Quaternion<float> q = filter.getAttitude();
}
```