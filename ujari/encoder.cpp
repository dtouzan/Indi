#if 0
    INDI Ujari Driver - Encoder
    Copyright (C) 2014 Jasem Mutlaq
    
    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA

#endif

#include <aiousb.h>

#include "encoder.h"
#include "ujari.h"

#define ENCODER_GROUP "Encoders"

Encoder::Encoder(encoderType type, Ujari* scope)
{
        // Initially, not connected
        connection_status = -1;

        setType(type);
        telescope = scope;

        debug = false;
        simulation = false;
        verbose    = true;
}

Encoder::~Encoder()
{
}

Encoder::encoderType Encoder::getType() const
{
    return type;
}

void Encoder::setType(const encoderType &value)
{
    type = value;

    switch (type)
    {
        case RA_ENCODER:
        type_name = std::string("RA Encoder");
        SLAVE_ADDRESS = 1;
        break;

        case DEC_ENCODER:
        type_name = std::string("DEC Encoder");
        SLAVE_ADDRESS = 2;
        break;

        case DOME_ENCODER:
        type_name = std::string("Dome Encoder");
        SLAVE_ADDRESS = 3;
        break;

    }

}
unsigned int Encoder::getEncoderValue() const
{
    return encoderValue;
}

void Encoder::setEncoderValue(unsigned int value)
{
    encoderValue = value;
}
double Encoder::getEncoderAngle() const
{
    return encoderAngle;
}

void Encoder::setEncoderAngle(double value)
{
    encoderAngle = value;
}
double Encoder::getTicksToDegreeRatio() const
{
    return ticksToDegreeRatio;
}

void Encoder::setTicksToDegreeRatio(double value)
{
    ticksToDegreeRatio = value;
}

/****************************************************************
**
**
*****************************************************************/
bool Encoder::initProperties()
{
    IUFillNumber(&encoderSettingsN[EN_HOME_POSITION], "HOME_POSITION", "Home Position", "%g", 0, 1000000, 1000, 0);
    IUFillNumber(&encoderSettingsN[EN_HOME_OFFSET], "HOME_OFFSET", "Home Offset", "%g", 0, 1000000, 1000, 0);
    IUFillNumber(&encoderSettingsN[EN_TICKS_DEGREES_RATIO], "TICKS_DEGREE_RATIO", "T/D Ratio", "%g", 0, 1000000, 1000, 0);

    IUFillNumber(&encoderValueN[EN_RAW_VALUE], "ENCODER_RAW_VALUE", "Value", "%g", 0, 1000000, 1000, 0);
    IUFillNumber(&encoderValueN[EN_ANGLE_VALUE], "ENCODER_ANGLE", "Angle", "%g", 0, 1000000, 1000, 0);

    switch (type)
    {
        case RA_ENCODER:
         IUFillNumberVector(&encoderSettingsNP, encoderSettingsN, 3, telescope->getDeviceName(), "RA_SETTINGS", "RA Settings", ENCODER_GROUP, IP_RW, 0, IPS_IDLE);
         IUFillNumberVector(&encoderValueNP, encoderValueN, 2, telescope->getDeviceName(), "RA_VALUES", "RA", ENCODER_GROUP, IP_RO, 0, IPS_IDLE);
        break;

        case DEC_ENCODER:
        IUFillNumberVector(&encoderSettingsNP, encoderSettingsN, 3, telescope->getDeviceName(), "DEC_SETTINGS", "DEC Settings", ENCODER_GROUP, IP_RW, 0, IPS_IDLE);
        IUFillNumberVector(&encoderValueNP, encoderValueN, 2, telescope->getDeviceName(), "DEC_VALUES", "DEC", ENCODER_GROUP, IP_RO, 0, IPS_IDLE);
        break;

        case DOME_ENCODER:
        IUFillNumberVector(&encoderSettingsNP, encoderSettingsN, 3, telescope->getDeviceName(), "DOME_SETTINGS", "Dome Settings", ENCODER_GROUP, IP_RW, 0, IPS_IDLE);
        IUFillNumberVector(&encoderValueNP, encoderValueN, 2, telescope->getDeviceName(), "DOME_VALUES", "Dome", ENCODER_GROUP, IP_RO, 0, IPS_IDLE);
        break;

    }

    return true;

}

/****************************************************************
**
**
*****************************************************************/
bool Encoder::connect()
{
    if (simulation)
    {
        DEBUGFDEVICE(telescope->getDeviceName(),INDI::Logger::DBG_SESSION, "%s: Simulating connecting to NI6509 board.", type_name.c_str());
        connection_status = 0;
        return true;
    }


    unsigned long result = AIOUSB_Init();
    if( result != AIOUSB_SUCCESS )
    {
        DEBUGFDEVICE(telescope->getDeviceName(),INDI::Logger::DBG_ERROR, "%s encoder: Can't initialize AIOUSB USB device.", type_name.c_str());
        return false;
    }

    //AIOUSB_ListDevices();

    // All input
    result = DIO_Configure(diOnly, AIOUSB_FALSE, 0, 0);

    if( result != AIOUSB_SUCCESS )
    {
        DEBUGFDEVICE(telescope->getDeviceName(),INDI::Logger::DBG_ERROR, "%s encoder: failed to configure all ports as input.", type_name.c_str());
        return false;
    }

    connection_status = 0;
    return true;
}

/****************************************************************
**
**
*****************************************************************/
void Encoder::disconnect()
{
    connection_status = -1;

    if (simulation)
        return;


    AIOUSB_CloseDevice(diOnly);
}

/****************************************************************
**
**
*****************************************************************/
void Encoder::ISGetProperties()
{

}

/****************************************************************
**
**
*****************************************************************/
bool Encoder::updateProperties(bool connected)
{
    if (connected)
    {
        telescope->defineNumber(&encoderSettingsNP);
        telescope->defineNumber(&encoderValueNP);
    }
    else
    {
        telescope->deleteProperty(encoderSettingsNP.name);
        telescope->deleteProperty(encoderValueNP.name);
    }
}

/****************************************************************
**
**
*****************************************************************/
bool Encoder::ISNewNumber (const char *dev, const char *name, double values[], char *names[], int n)
{
    if (!strcmp(encoderSettingsNP.name, name))
    {
        if (IUUpdateNumber(&encoderSettingsNP, values, names, n) < 0)
            encoderSettingsNP.s = IPS_ALERT;
        else
            encoderSettingsNP.s = IPS_OK;

        IDSetNumber(&encoderSettingsNP, NULL);

        return true;
    }

    return false;

}

/****************************************************************
**
**
*****************************************************************/
bool Encoder::ISNewText (const char *dev, const char *name, char *texts[], char *names[], int n)
{

    return false;
}

/****************************************************************
**
**
*****************************************************************/
unsigned long Encoder::GetEncoder()  throw (UjariError)
{
    return static_cast<unsigned long>(encoderValueN[EN_RAW_VALUE].value);

}

/****************************************************************
**
**
*****************************************************************/
unsigned long Encoder::GetEncoderZero()
{
    return static_cast<unsigned long>(encoderSettingsN[EN_HOME_POSITION].value+encoderSettingsN[EN_HOME_OFFSET].value);
}

/****************************************************************
**
**
*****************************************************************/
unsigned long Encoder::GetEncoderTotal()
{
    return static_cast<unsigned long>(encoderValueN[EN_RAW_VALUE].max);

}

/****************************************************************
**
**
*****************************************************************/
unsigned long Encoder::GetEncoderHome()
{
    return static_cast<unsigned long>(encoderSettingsN[EN_HOME_POSITION].value+encoderSettingsN[EN_HOME_OFFSET].value);

}
