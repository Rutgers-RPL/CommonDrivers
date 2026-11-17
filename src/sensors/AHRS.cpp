#include "AHRS.h"
#include <cmath>

// Constructor: Initializes default filter gain and resets orientation
AHRS::AHRS() : filterGain(0.1f) { reset(); }

// begin(): Initializes filter parameters or hardware states if needed
bool AHRS::begin() {
    reset();
    return true;
}

// reset(): Resets orientation quaternion back to identity (level on pad)
void AHRS::reset() {
    currentOrientation.w = 1.0f;
    currentOrientation.x = 0.0f;
    currentOrientation.y = 0.0f;
    currentOrientation.z = 0.0f;
}

// setFilterGain(): Sets the tuning multiplier for sensor correction speed
void AHRS::setFilterGain(float gain) { filterGain = gain; }

// getQuaternion(): Returns the current orientation estimate
Quaternion AHRS::getQuaternion() const { return currentOrientation; }

// getEulerAngles(): Converts current quaternion to Roll, Pitch, and Yaw
// (radians)
void AHRS::getEulerAngles(float& roll, float& pitch, float& yaw) const {
    // Roll (x-axis rotation)
    float sinr_cosp = 2.0f * (currentOrientation.w * currentOrientation.x +
                              currentOrientation.y * currentOrientation.z);
    float cosr_cosp =
        1.0f - 2.0f * (currentOrientation.x * currentOrientation.x +
                       currentOrientation.y * currentOrientation.y);
    roll = std::atan2(sinr_cosp, cosr_cosp);

    // Pitch (y-axis rotation)
    float sinp = 2.0f * (currentOrientation.w * currentOrientation.y -
                         currentOrientation.z * currentOrientation.x);
    if (std::abs(sinp) >= 1.0f)
        pitch = std::copysign(3.1415926535f / 2.0f,
                              sinp); // Use 90 degrees if out of bounds
    else
        pitch = std::asin(sinp);

    // Yaw (z-axis rotation / heading)
    float siny_cosp = 2.0f * (currentOrientation.w * currentOrientation.z +
                              currentOrientation.x * currentOrientation.y);
    float cosy_cosp =
        1.0f - 2.0f * (currentOrientation.y * currentOrientation.y +
                       currentOrientation.z * currentOrientation.z);
    yaw = std::atan2(siny_cosp, cosy_cosp);
}

// normalizeQuaternion(): Keeps quaternion magnitude strictly equal to 1.0 to
// prevent drift
void AHRS::normalizeQuaternion(Quaternion& q) {
    float magnitude = std::sqrt(q.w * q.w + q.x * q.x + q.y * q.y + q.z * q.z);
    if (magnitude > 0.0f) {
        q.w /= magnitude;
        q.x /= magnitude;
        q.y /= magnitude;
        q.z /= magnitude;
    }
}

// update(): Core sensor fusion loop combining gyro prediction and conditional
// sensor corrections
void AHRS::update(const Vector3D& gyro, const Vector3D& accel,
                  const Vector3D& mag, float dt, FlightPhase phase) {
    // 1. Gyroscope Integration (Prediction Step): Convert body rates into
    // quaternion derivative
    float qDotW =
        0.5f * (-currentOrientation.x * gyro.x - currentOrientation.y * gyro.y -
                currentOrientation.z * gyro.z);
    float qDotX =
        0.5f * (currentOrientation.w * gyro.x + currentOrientation.y * gyro.z -
                currentOrientation.z * gyro.y);
    float qDotY =
        0.5f * (currentOrientation.w * gyro.y - currentOrientation.x * gyro.z +
                currentOrientation.z * gyro.x);
    float qDotZ =
        0.5f * (currentOrientation.w * gyro.z + currentOrientation.x * gyro.y -
                currentOrientation.y * gyro.x);

    // Apply time step to update quaternion state
    currentOrientation.w += qDotW * dt;
    currentOrientation.x += qDotX * dt;
    currentOrientation.y += qDotY * dt;
    currentOrientation.z += qDotZ * dt;

    // 2. Conditional Sensor Correction (The Flight Phase Guard)
    // If powered ascent is active, skip accelerometer correction to avoid
    // thrust-acceleration distortion trap
    if (phase == FlightPhase::POWERED_ASCENT) {
        // Rely purely on gyro propagation during heavy thrust phases
    } else {
        // Pad or Coast phase: Safe to apply gravity tilt correction from
        // accelerometer
        computeGravityAndMagneticError(accel, mag, phase);
    }

    // 3. Normalize final orientation state
    normalizeQuaternion(currentOrientation);
}

// computeGravityAndMagneticError(): Fuses magnetometer and conditional
// accelerometer feedback
void AHRS::computeGravityAndMagneticError(const Vector3D& accel,
                                          const Vector3D& mag,
                                          FlightPhase phase) {
    // Placeholder for vector cross-product error calculation matching
    // gravity/magnetic references (This integrates your sensor feedback vectors
    // into error proportional terms scaled by filterGain)
    (void)accel;
    (void)mag;
    (void)phase;
}