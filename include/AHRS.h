#ifndef AHRS_H
#define AHRS_H

#include <cstdint>

struct Vector3D {
    float x; // X-axis component
    float y; // Y-axis component
    float z; // Z-axis component
};

// Bundles 4 scalar values into a single memory block to represent a 3D rotation
// in quaternion form.
struct Quaternion {
    float w, x, y, z; // Hypercomplex comoponenets: 'w' is scalar magnitude
                      // x,y,z define the axis of rotation vector
    Quaternion()
        : w(1.0f), x(0.0f), y(0.0f), z(0.0f) {
    } // Default constructor initializes to identity quaternion
    Quaternion(float w_v, float x_v, float y_v, float z_v)
        : w(w_v), x(x_v), y(y_v), z(z_v) {
    } // parameterized constructor for instant assignment during teal-time
      // updates
};

// Defines the operational states of the rocket to control sensor weighting
enum class FlightPhase {
    PAD_OR_STATIC,  // Rocket is stationary on the pad; gravity is clean;
                    // accelerometer tilt is trusted
    POWERED_ASCENT, // Rocket is under thrust; accelerometer is dominated by
                    // thrust(disabled); gyroscope is trusted
    COAST_PHASE,    // Rocket is coasting, low acceleration environment;
                    // accelerometer is re-enabled
};

class AHRS {
public:
    AHRS();  // constructor initializes the AHRS systeem with default values
    ~AHRS(); // destructor for AHRS

    bool
    begin(); // Initalizes the filter parameters, gains, and internal states
    void reset(); // Resets the filter to its initial state, clearing any
                  // accumulated errors or biases
    void update(
        const Vector3D& accel, const Vector3D& gyro, const Vector3D& mag,
        float dt,
        FlightPhase phase); // Updates the AHRS filter with new sensor data and
                            // time delta, adjusting for current flight phase
    Quaternion
    getQuaternion() const; // Returns the current 4D orientation as a quaternion
    void getEulerAngles(float& roll, float& pitch, float& yaw) const;
    // Converts the current quaternion to roll, pitch, and yaw angles
    void setFilterGain(float gain); // Sets the filter tuning gain that controls
                                    // the sensor correction aggressiveness

private:
    // Running orietation state vector (Quaternion)
    Quaternion currentOrientation;
    // Convergance rate multiplier for sensor corrections
    float filterGain;
    // Mathematical helper to normalize the quaternion magnitude to 1.0
    void normalizeQuaternion(Quaternion& q);
    // Internal error correction logic for gravity and magenetic vectors
    void computeGravityAndMagneticError(const Vector3D& accel,
                                        const Vector3D& mag, FlightPhase phase);
};

#endif // AHRS_H