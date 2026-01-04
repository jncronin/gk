#pragma once

#include <Eigen>
namespace IMU_EKF
{
template <typename precision, typename Derived>
Eigen::Matrix<precision, 3, 3> toCrossMatrix(const Eigen::DenseBase<Derived> &v)
{
    Eigen::Matrix<precision, 3, 3> crossMatrix;
    // eq. 5
    crossMatrix << 0.0, -v(2), v(1), v(2), 0.0, -v(0), -v(1), v(0), 0.0;
    return crossMatrix;
}

template <typename precision, typename Derived>
Eigen::Matrix<precision, 3, 3> toRotationMatrix(const Eigen::MatrixBase<Derived> &v)
{
    Eigen::Matrix<precision, 3, 3> rotationMatrix;
    // eq. 20
    precision norm_sqr = v(0) * v(0) + v(1) * v(1) + v(2) * v(2);
    Eigen::Matrix<precision, 3, 3> vvT = v * v.transpose();
    rotationMatrix = Eigen::Matrix<precision, 3, 3>::Identity() - toCrossMatrix<precision>(v) - 0.5 * (norm_sqr * Eigen::Matrix<precision, 3, 3>::Identity() - vvT);
    return rotationMatrix;
}
} // namespace IMU_EKF