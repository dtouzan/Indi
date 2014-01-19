/*!
 * \file skywatcherAPI.cpp
 *
 * \author Roger James
 * \author Gerry Rozema
 * \author Jean-Luc Geehalel
 * \date 13th November 2013
 *
 * This file contains the definitions for a C++ implementatiom of the Skywatcher API.
 * It is based on work from three sources.
 * A C++ implementation of the API by Roger James.
 * The indi_eqmod driver by Jean-Luc Geehalel.
 * The synscanmount driver by Gerry Rozema.
 */

#ifndef SKYWATCHERAPI_H
#define SKYWATCHERAPI_H

#include <cmath>
#include <string>

#define INDI_DEBUG_LOGGING
#ifdef INDI_DEBUG_LOGGING
#include "indibase/inditelescope.h"
#define MYDEBUG(priority, msg) INDI::Logger::getInstance().print(pChildTelescope->getDeviceName(), priority, __FILE__, __LINE__, msg)
#define MYDEBUGF(priority, msg, ...) INDI::Logger::getInstance().print(pChildTelescope->getDeviceName(), priority, __FILE__, __LINE__,  msg, __VA_ARGS__)
#else
#define MYDEBUG(priority, msg)
#define MYDEBUGF(priority, msg, ...)
#endif

struct AXISSTATUS
{
    AXISSTATUS() : FullStop(false), Slewing(false), SlewingTo(false), SlewingForward(false), HighSpeed(false), NotInitialized(true) {}
    bool FullStop;
    bool Slewing;
    bool SlewingTo;
    bool SlewingForward;
    bool HighSpeed;
    bool NotInitialized;

    void SetFullStop();
    void SetSlewing(bool forward, bool highspeed);
    void SetSlewingTo(bool forward, bool highspeed);
};

class SkywatcherAPI
{
public:
    enum AXISID { AXIS1 = 0, AXIS2 = 1 };

    static const double SIDEREALRATE = (2 * M_PI / 86164.09065); // Radians per second
    static const double MAX_SPEED = 500; // Radians per second
    static const double LOW_SPEED_MARGIN; // Radians per second

    SkywatcherAPI();
    virtual ~SkywatcherAPI();

    /// \brief Convert angle to microsteps
    /// \param[in] Axis - The axis to use.
    /// \param[in] AngleInRadians - the angle in radians.
    /// \return the number of microsteps
    long AngleToStep(AXISID Axis, double AngleInRadians);

    long BCDstr2long(std::string &String);
    bool CheckIfDCMotor();

    /// \brief Set the GearRatio status variable to the number of microsteps
    /// for a 360 degree revolution of the axis.
    /// \param[in] Axis - The axis to use.
    /// \return false failure
    bool GetGridPerRevolution(AXISID Axis);

    /// \brief Set the HighSpeedRatio status variable to the ratio between
    /// high and low speed stepping modes.
    bool GetHighSpeedRatio(AXISID Axis);

    bool GetMotorBoardVersion(AXISID Axis);
    bool GetPosition(AXISID Axis);
    bool GetStatus(AXISID Axis);

    /// \brief Set the StepTimerFreq status variable to fixed PIC timer interrupt
    /// frequency (ticks per second).
    /// \return false failure
    bool GetTimerInterruptFreq(AXISID Axis);

    bool InitializeMC();
    bool InitMount();
    bool GetPECPeriod(AXISID Axis);
    bool InstantStop(AXISID Axis);
    void Long2BCDstr(long Number, std::string &String);
    void PrepareForSlewing(AXISID Axis, double Speed);

    /// \brief Convert a speed in seconds per radian to microsteps per second
    /// \param[in] RateInSecondsPerRadian
    /// \return Microsteps per second
    long RadSpeedToInt(AXISID Axis, double RateInSecondsPerRadian);

    bool SetBreakPointIncrement(AXISID Axis, long StepsCount);
    bool SetBreakSteps(AXISID Axis, long NewBreakSteps);
    bool SetGotoTargetIncrement(AXISID Axis, long StepsCount);
    bool SetMotionMode(AXISID Axis, char Func, char Direction);
    bool SetPosition(AXISID Axis, double Position);
    void SetSerialPort(int port) { MyPortFD = port; }

    /// \brief Set the PIC internal divider variable which determines
    /// how many clock interrupts have to occur between each microstep
    bool SetStepPeriod(AXISID Axis, long ClockTicksPerMicrostep);

    bool SetSwitch(bool OnOff);
    void Slew(AXISID Axis, double SpeedInRadiansPerSecond);
    bool StartMotion(AXISID Axis);
    double StepToAngle(AXISID Axis, long Steps);
    bool Stop(AXISID Axis);
    bool TalkWithAxis(AXISID Axis, char Command, std::string& cmdDataStr, std::string& responseStr);

    // Skywatcher mount status variables
    long MCVersion; // Motor control board firmware version
    long MountCode;
    bool IsDCMotor;

    // Values from mount
    long GearRatio[2]; // Number of microsteps for 360 degree revolution
    long StepTimerFreq[2]; // The interrupt frequency of the axis PIC - this divided by
                            // the current StepPeriod gives the microstepping rate per second
    long HighSpeedRatio[2]; // This is the speed multiplier for high speed mode.
    long PESteps[2]; // Number of microsteps for one revolution of the worm gear.

    // Calculated values
    double FactorRadToStep[2];
    double FactorStepToRad[2];
    double FactorRadRateToInt[2];
    long LowSpeedGotoMargin[2];
    long BreakSteps[2];

    // Encoder values
    double CurrentPositions[2];
    double InitialPositions[2];

    AXISSTATUS AxesStatus[2];
    double SlewingSpeed[2];
private:
    enum TTY_ERROR { TTY_OK=0, TTY_READ_ERROR=-1, TTY_WRITE_ERROR=-2, TTY_SELECT_ERROR=-3, TTY_TIME_OUT=-4, TTY_PORT_FAILURE=-5, TTY_PARAM_ERROR=-6, TTY_ERRNO = -7};
    virtual int skywatcher_tty_read(int fd, char *buf, int nbytes, int timeout, int *nbytes_read) = 0;
//    virtual int skywatcher_tty_read_section(int fd, char *buf, char stop_char, int timeout, int *nbytes_read) = 0;
    virtual int skywatcher_tty_write(int fd, const char * buffer, int nbytes, int *nbytes_written) = 0;
//    virtual int skywatcher_tty_write_string(int fd, const char * buffer, int *nbytes_written) = 0;
//    virtual int skywatcher_tty_connect(const char *device, int bit_rate, int word_size, int parity, int stop_bits, int *fd) = 0;
//    virtual int skywatcher_tty_disconnect(int fd) = 0;
//    virtual void skywatcher_tty_error_msg(int err_code, char *err_msg, int err_msg_len) = 0;
//    virtual int skywatcher_tty_timeout(int fd, int timeout) = 0;*/
    int MyPortFD;

#ifdef INDI_DEBUG_LOGGING
public:
    INDI::Telescope *pChildTelescope;
#endif
};

#endif // SKYWATCHERAPI_H
