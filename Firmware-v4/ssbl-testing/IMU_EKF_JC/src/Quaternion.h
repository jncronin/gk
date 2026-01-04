#pragma once

#include <stdint.h>
#include <Eigen>
#include "utilities.h"

namespace IMU_EKF
{

enum QuaternionIndex
{
    v1,
    v2,
    v3,
    w
};

template <typename precision>
class Quaternion
{
public:
    Quaternion() : v1_(0), v2_(0), v3_(0), w_(1){};
    Quaternion(precision v1, precision v2, precision v3, precision w) : v1_(v1), v2_(v2), v3_(v3), w_(w){};

    precision operator[](QuaternionIndex idx) const
    {
        switch (idx)
        {
        case v1:
            return v1_;
        case v2:
            return v2_;
        case v3:
            return v3_;
        case w:
            return w_;
        default:
            return v1_;
        }
    }

    Quaternion<precision> &operator+=(const Quaternion<precision> r)
    {
        v1_ += r[v1];
        v2_ += r[v2];
        v3_ += r[v3];
        w_ += r[w];

        return *this;
    }

    Eigen::Matrix<precision, 3, 1> vec() const
    {
        Eigen::Matrix<precision, 3, 1> vec;
        vec << v1, v2, v3;
        return vec;
    }

    Eigen::Matrix<precision, 3, 3> toRotationMatrix() const
    {
        Eigen::Matrix<precision, 3, 3> rotationMatrix;
        // eq. 4
        // rotationMatrix = (w_ * w_ - v1_ * v1_ - v2_ * v2_ - v3_ * v3_) * Eigen::Matrix<precision, 3, 3>::Identity() - 2 * w_ * toCrossMatrix<precision>(vec()) + 2 * vec() * vec().transpose();
        // from https://www.euclideanspace.com/maths/geometry/rotations/conversions/quaternionToMatrix/index.htm
        rotationMatrix << 1.0 - 2.0 * v2_ * v2_ - 2.0 * v3_ * v3_, 2.0 * v1_ * v2_ - 2.0 * v3_ * w_, 2.0 * v1_ * v3_ + 2.0 * v2_ * w_,
            2.0 * v1_ * v2_ + 2.0 * v3_ * w_, 1.0 - 2.0 * v1_ * v1_ - 2.0 * v3_ * v3_, 2.0 * v2_ * v3_ - 2.0 * v1_ * w_,
            2.0 * v1_ * v3_ - 2.0 * v2_ * w_, 2.0 * v2_ * v3_ + 2.0 * v1_ * w_, 1.0 - 2.0 * v1_ * v1_ - 2.0 * v2_ * v2_;
        return rotationMatrix.transpose();
    }

    Eigen::Matrix<precision, 3, 1> toEulerAngles() const
    {
        Eigen::Matrix<precision, 3, 1> angles;
        // from https://en.wikipedia.org/wiki/Conversion_between_quaternions_and_Euler_angles
        // rotation sequence R = Rx * Ry * Rz
        // roll (x-axis rotation)
        precision sinr_cosp = 2 * (v2_ * v3_ - w_ * v1_);
        precision cosr_cosp = 1 - 2 * (v1_ * v1_ + v2_ * v2_);
        angles[0] = std::atan2(sinr_cosp, cosr_cosp);

        // pitch (y-axis rotation)
        precision sinp = -2 * (w_ * v2_ + v3_ * v1_);
        if (std::abs(sinp) >= 1)
            angles[1] = std::copysign(M_PI / 2, sinp); // use 90 degrees if out of range
        else
            angles[1] = std::asin(sinp);

        // yaw (z-axis rotation)
        precision siny_cosp = 2 * (v1_ * v2_ - w_ * v3_);
        precision cosy_cosp = 1 - 2 * (v2_ * v2_ + v3_ * v3_);
        angles[2] = std::atan2(siny_cosp, cosy_cosp);

        return angles;
    }

    void normalize()
    {
        precision norm = sqrt(v1_ * v1_ + v2_ * v2_ + v3_ * v3_ + w_ * w_);
        if (std::abs(norm) > 1e-8)
        {
            v1_ /= norm;
            v2_ /= norm;
            v3_ /= norm;
            w_ /= norm;
        }
    }

protected:
    precision v1_;
    precision v2_;
    precision v3_;
    precision w_;
};

template <typename precision>
Quaternion<precision> operator*(const Quaternion<precision> &l, const Quaternion<precision> &r)
{
    // eq. 6
    precision nv1 = l[w] * r[v1] + r[w] * l[v1] - l[v2] * r[v3] + l[v3] * r[v2];
    precision nv2 = l[w] * r[v2] + r[w] * l[v2] - l[v3] * r[v1] + l[v1] * r[v3];
    precision nv3 = l[w] * r[v3] + r[w] * l[v3] - l[v1] * r[v2] + l[v2] * r[v1];
    precision nw = l[w] * r[w] - l[v1] * r[v1] - l[v2] * r[v2] - l[v3] * r[v3];
    return Quaternion<precision>(nv1, nv2, nv3, nw);
}

Quaternion<float> operator*(float l, const Quaternion<float> &r)
{
    return Quaternion<float>(r[v1] * l, r[v2] * l, r[v3] * l, r[w] * l);
}

Quaternion<double> operator*(double l, const Quaternion<double> &r)
{
    return Quaternion<double>(r[v1] * l, r[v2] * l, r[v3] * l, r[w] * l);
}

template <typename precision>
Quaternion<precision> operator+(const Quaternion<precision> &l, const Quaternion<precision> &r)
{
    return Quaternion<precision>(l[v1] * r[v1], l[v2] * r[v2], l[v3] * r[v3], l[w] * r[w]);
}
} // namespace IMU_EKF