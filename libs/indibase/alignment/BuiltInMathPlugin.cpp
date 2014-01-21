#include "BuiltInMathPlugin.h"

#include <gsl/gsl_permutation.h>
#include <gsl/gsl_linalg.h>
#include <gsl/gsl_blas.h>

namespace INDI {
namespace AlignmentSubsystem {

// Constructors

BuiltInMathPlugin::BuiltInMathPlugin()
{
    pActualToApparentTransform = gsl_matrix_alloc(3,3);
    pApparentToActualTransform = gsl_matrix_alloc(3,3);
}

// Destructor

BuiltInMathPlugin::~BuiltInMathPlugin()
{
    gsl_matrix_free(pActualToApparentTransform);
    gsl_matrix_free(pApparentToActualTransform);
}

// Public methods

bool BuiltInMathPlugin::Initialise()
{
    /// See how many entries there are in the in memory database.
    /// - If just one use a hint to mounts approximate alignment, this can either be ZENITH,
    /// NORTH_CELESTIAL_POLE or SOUTH_CELESTIAL_POLE. The hint is used to make a dummy second
    /// entry. A dummy third entry is computed from the cross product of the first two. A transform
    /// matrix is then computed.
    /// - If two make the dummy third entry and compute a transform matrix.
    /// - If three compute a transform matrix.
    /// - If four or more compute a convex hull, then matrices for each
    /// triangular facet of the hull.
    switch (GetAlignmentDatabase().size())
    {
        case 0:
            return false;

        case 1:
        {
#if 0
            // Compute local horizontal coordinate offsets
            AlignmentDatabaseEntry& Entry = GetAlignmentDatabase()[0];
            ln_equ_posn RaDec;
            ln_lnlat_posn Position;
            if (!GetDatabaseReferencePosition(Position))
                return false;
            RaDec.dec = Entry.Declination;
            RaDec.ra = Entry.RightAscension;
            switch (ApproximateMountAlignment)
            {
                case ZENITH:
                    ln_hrz_posn AltAz;
                    ln_hrz_posn MountAltAz;
                   // Probably an altaz goto or a dobsonian
                    // Translate sync point RA/DEC into alt/az (clockwise from South) for the
                    // date and time of the sync point observation.
                    ln_get_hrz_from_equ(&RaDec, &Position, Entry.ObservationJulianDate, &AltAz);
                    // Translate the mount coordinates from the sync point into similar clockwise coordinate space
                    AltitudeAzimuthFromNormalisedDirectionVector(Entry.TelescopeDirection, MountAltAz);
                    SinglePointOffsetsAltAz.alt = AltAz.alt - MountAltAz.alt;
                    SinglePointOffsetsAltAz.az = AltAz.az - MountAltAz.az;
                    break;

                case NORTH_CELESTIAL_POLE:
                {
                    // THIS IS RUBBISH!!!!!!!! I need to review it.
                    // Equatorial in the northern hemisphere
                    ln_equ_posn AdjustedEquatorialCoordinate;
                    ln_equ_posn MountEquatorialCoordinate;
                    double GreenwichMeanSiderealTime = ln_get_mean_sidereal_time(Entry.ObservationJulianDate);
                    // Convert time to degrees
                    GreenwichMeanSiderealTime *= 360.0 / 24.0;
                    // Convert right ascension to local hour angle
                    // LHA = GMST + Longitude (positive east) - RA
                    AdjustedEquatorialCoordinate.ra = GreenwichMeanSiderealTime + Position.lng - RaDec.ra;
                    // N.B. this has converted the ra value from ANTI_CLOCKWISE to CLOCKWISE

                    // Convert declination to elevation measured from equatorial plane towards the north celestial pole
                    // I am really guessing here !!!!!
                    AdjustedEquatorialCoordinate.dec = 90.0 - RaDec.dec;


                    LocalHourAngleDeclinationFromTelescopeDirectionVector(Entry.TelescopeDirection, MountEquatorialCoordinate);
                    SinglePointsOffsetsRaDec.ra =  AdjustedEquatorialCoordinate.ra - MountEquatorialCoordinate.ra;
                    SinglePointsOffsetsRaDec.dec =  AdjustedEquatorialCoordinate.dec - MountEquatorialCoordinate.dec;
                    break;
                }

                case SOUTH_CELESTIAL_POLE:
                    // My brain has stalled. I assume I reverse the rotation of the hour angle calc
                    // and fudge the declination to elevation measured from the equatorial plane towards the south celestial pole
                    return false;

            }
            return true;
#else
            // Compute local horizontal coordinate offsets
            AlignmentDatabaseEntry& Entry1 = GetAlignmentDatabase()[0];
            ln_equ_posn RaDec;
            ln_hrz_posn ActualSyncPoint1;
            ln_lnlat_posn Position;
            if (!GetDatabaseReferencePosition(Position))
                return false;
            RaDec.dec = Entry1.Declination;
            RaDec.ra = Entry1.RightAscension;
            ln_get_hrz_from_equ(&RaDec, &Position, Entry1.ObservationJulianDate, &ActualSyncPoint1);
            // Now express this coordinate as normalised direction vectors (a.k.a direction cosines)
            TelescopeDirectionVector ActualDirectionCosine1 = TelescopeDirectionVectorFromAltitudeAzimuth(ActualSyncPoint1);
            // Generate the second dummy sync point
            ln_hrz_posn DummyAltAz;
            ln_equ_posn DummyRaDec;
            TelescopeDirectionVector DummyActualDirectionCosine2;
            TelescopeDirectionVector DummyApparentDirectionCosine2;
            switch (ApproximateMountAlignment)
            {
                case ZENITH:
                    DummyAltAz.alt = 90.0;
                    DummyAltAz.az = 0.0;
                    break;

                case NORTH_CELESTIAL_POLE:
                    DummyRaDec.ra = 0.0;
                    DummyRaDec.dec = 0.0;
                    ln_get_hrz_from_equ(&RaDec, &Position, ln_get_julian_from_sys(), &DummyAltAz);
                    break;

                case SOUTH_CELESTIAL_POLE:
                    DummyRaDec.ra = 0.0;
                    DummyRaDec.dec = 180.0;
                    ln_get_hrz_from_equ(&RaDec, &Position, ln_get_julian_from_sys(), &DummyAltAz);
                    break;
            }
            DummyActualDirectionCosine2 = TelescopeDirectionVectorFromAltitudeAzimuth(DummyAltAz);
            // Cheat - make actual and apparent the same
            DummyApparentDirectionCosine2 = DummyActualDirectionCosine2;
            CalculateTAKIMatrices(ActualDirectionCosine1, DummyActualDirectionCosine2, ActualDirectionCosine1 * DummyActualDirectionCosine2,
                                Entry1.TelescopeDirection, DummyApparentDirectionCosine2, Entry1.TelescopeDirection * DummyApparentDirectionCosine2,
                                pActualToApparentTransform, pApparentToActualTransform);
            return true;
#endif
        }
        case 2:
        {
            // First compute local horizontal coordinates for the two sync points
            AlignmentDatabaseEntry& Entry1 = GetAlignmentDatabase()[0];
            AlignmentDatabaseEntry& Entry2 = GetAlignmentDatabase()[1];
            ln_hrz_posn ActualSyncPoint1;
            ln_hrz_posn ActualSyncPoint2;
            ln_equ_posn RaDec1;
            ln_equ_posn RaDec2;
            RaDec1.dec = Entry1.Declination;
            RaDec1.ra = Entry1.RightAscension;
            RaDec2.dec = Entry2.Declination;
            RaDec2.ra = Entry2.RightAscension;
            ln_lnlat_posn Position;
            if (!GetDatabaseReferencePosition(Position))
                return false;
            ln_get_hrz_from_equ(&RaDec1, &Position, Entry1.ObservationJulianDate, &ActualSyncPoint1);
            ln_get_hrz_from_equ(&RaDec2, &Position, Entry2.ObservationJulianDate, &ActualSyncPoint2);

            // Now express these coordinates as normalised direction vectors (a.k.a direction cosines)
            TelescopeDirectionVector ActualDirectionCosine1 = TelescopeDirectionVectorFromAltitudeAzimuth(ActualSyncPoint1);
            TelescopeDirectionVector ActualDirectionCosine2 = TelescopeDirectionVectorFromAltitudeAzimuth(ActualSyncPoint2);

            // The third direction vectors is generated by taking the cross product of the first two
            CalculateTAKIMatrices(ActualDirectionCosine1, ActualDirectionCosine2, ActualDirectionCosine1 * ActualDirectionCosine2,
                                Entry1.TelescopeDirection, Entry2.TelescopeDirection, Entry1.TelescopeDirection * Entry2.TelescopeDirection,
                                pActualToApparentTransform, pApparentToActualTransform);
            return true;
        }

        case 3:
        {
            // First compute local horizontal coordinates for the three sync points
            AlignmentDatabaseEntry& Entry1 = GetAlignmentDatabase()[0];
            AlignmentDatabaseEntry& Entry2 = GetAlignmentDatabase()[1];
            AlignmentDatabaseEntry& Entry3 = GetAlignmentDatabase()[2];
            ln_hrz_posn ActualSyncPoint1;
            ln_hrz_posn ActualSyncPoint2;
            ln_hrz_posn ActualSyncPoint3;
            ln_equ_posn RaDec1;
            ln_equ_posn RaDec2;
            ln_equ_posn RaDec3;
            RaDec1.dec = Entry1.Declination;
            RaDec1.ra = Entry1.RightAscension;
            RaDec2.dec = Entry2.Declination;
            RaDec2.ra = Entry2.RightAscension;
            RaDec3.dec = Entry3.Declination;
            RaDec3.ra = Entry3.RightAscension;
            ln_lnlat_posn Position;
            if (!GetDatabaseReferencePosition(Position))
                return false;
            ln_get_hrz_from_equ(&RaDec1, &Position, Entry1.ObservationJulianDate, &ActualSyncPoint1);
            ln_get_hrz_from_equ(&RaDec2, &Position, Entry2.ObservationJulianDate, &ActualSyncPoint2);
            ln_get_hrz_from_equ(&RaDec3, &Position, Entry3.ObservationJulianDate, &ActualSyncPoint3);

            // Now express these coordinates as normalised direction vectors (a.k.a direction cosines)
            TelescopeDirectionVector ActualDirectionCosine1 = TelescopeDirectionVectorFromAltitudeAzimuth(ActualSyncPoint1);
            TelescopeDirectionVector ActualDirectionCosine2 = TelescopeDirectionVectorFromAltitudeAzimuth(ActualSyncPoint2);
            TelescopeDirectionVector ActualDirectionCosine3 = TelescopeDirectionVectorFromAltitudeAzimuth(ActualSyncPoint3);

            CalculateTAKIMatrices(ActualDirectionCosine1, ActualDirectionCosine2, ActualDirectionCosine3,
                                Entry1.TelescopeDirection, Entry2.TelescopeDirection, Entry3.TelescopeDirection,
                                pActualToApparentTransform, pApparentToActualTransform);
            return true;
        }


        default:
            // Compute Hull etc.
            return true;
    }
}

bool BuiltInMathPlugin::TransformCelestialToTelescope(const double RightAscension, const double Declination, TelescopeDirectionVector& ApparentTelescopeDirectionVector)
{
    ln_equ_posn ActualRaDec;
    ActualRaDec.ra = RightAscension;
    ActualRaDec.dec = Declination;
    ln_lnlat_posn Position;
    if (!GetDatabaseReferencePosition(Position)) // Should check that this the same as the current observing position
        return false;

    switch (GetAlignmentDatabase().size())
    {
        case 0:
            // No alignment points
            return false;

#if 0
        case 1:
            // 1 alignment point. Use the stored single point offsets
            switch (ApproximateMountAlignment)
            {
                case ZENITH:
                    ln_hrz_posn ActualAltAz;
                    ln_hrz_posn ApparentAltAz;
                    ln_lnlat_posn Position;
                    if (!GetDatabaseReferencePosition(Position))
                        return false;
                    // Hmmm.. should I get the julian date from the observatory clock
                    ln_get_hrz_from_equ(&ActualRaDec, &Position, ln_get_julian_from_sys(), &ActualAltAz);

                    ApparentAltAz.alt = ActualAltAz.alt - SinglePointOffsetsAltAz.alt;
                    ApparentAltAz.az = ActualAltAz.az - SinglePointOffsetsAltAz.az;

                    TelescopeDirectionVector = TelescopeDirectionVectorFromAltitudeAzimuth(ApparentAltAz);
                    break;

                case NORTH_CELESTIAL_POLE:
                case SOUTH_CELESTIAL_POLE: // TODO Check if I need to do anything different here.
                    break;
            }
            break;
#else
        case 1:
#endif
        case 2:
        case 3:
        {
            ln_hrz_posn ActualAltAz;
            ln_hrz_posn ApparentAltAz;
            ln_get_hrz_from_equ(&ActualRaDec, &Position, ln_get_julian_from_sys(), &ActualAltAz);
            TelescopeDirectionVector ActualVector = TelescopeDirectionVectorFromAltitudeAzimuth(ActualAltAz);
            gsl_vector *pGSLActualVector = gsl_vector_alloc(3);
            gsl_vector_set(pGSLActualVector, 0, ActualVector.x);
            gsl_vector_set(pGSLActualVector, 1, ActualVector.y);
            gsl_vector_set(pGSLActualVector, 2, ActualVector.z);
            gsl_vector *pGSLApparentVector = gsl_vector_alloc(3);
            MatrixVectorMultipy(pActualToApparentTransform, pGSLActualVector, pGSLApparentVector);
            ApparentTelescopeDirectionVector.x = gsl_vector_get(pGSLApparentVector, 0);
            ApparentTelescopeDirectionVector.y = gsl_vector_get(pGSLApparentVector, 1);
            ApparentTelescopeDirectionVector.z = gsl_vector_get(pGSLApparentVector, 2);
            ApparentTelescopeDirectionVector.Normalise();
            gsl_vector_free(pGSLActualVector);
            gsl_vector_free(pGSLApparentVector);
            break;
        }

        default:
            // Convex hull stuff to go in here
            return false;
    }
    return true;
}

bool BuiltInMathPlugin::TransformTelescopeToCelestial(const TelescopeDirectionVector& ApparentTelescopeDirectionVector, double& RightAscension, double& Declination)
{
    ln_lnlat_posn Position;
    if (!GetDatabaseReferencePosition(Position)) // Should check that this the same as the current observing position
        return false;

    switch (GetAlignmentDatabase().size())
    {
        case 0:
            // No alignment points
            return false;

        case 1:
        case 2:
        case 3:
        {
            gsl_vector *pGSLApparentVector = gsl_vector_alloc(3);
            gsl_vector_set(pGSLApparentVector, 0, ApparentTelescopeDirectionVector.x);
            gsl_vector_set(pGSLApparentVector, 1, ApparentTelescopeDirectionVector.y);
            gsl_vector_set(pGSLApparentVector, 2, ApparentTelescopeDirectionVector.z);
            gsl_vector *pGSLActualVector = gsl_vector_alloc(3);
            MatrixVectorMultipy(pApparentToActualTransform, pGSLApparentVector, pGSLActualVector);
            TelescopeDirectionVector ActualTelescopeDirectionVector;
            ActualTelescopeDirectionVector.x = gsl_vector_get(pGSLActualVector, 0);
            ActualTelescopeDirectionVector.y = gsl_vector_get(pGSLActualVector, 1);
            ActualTelescopeDirectionVector.z = gsl_vector_get(pGSLActualVector, 2);
            ActualTelescopeDirectionVector.Normalise();
            ln_hrz_posn ActualAltAz;
            AltitudeAzimuthFromTelescopeDirectionVector(ActualTelescopeDirectionVector, ActualAltAz);
            ln_equ_posn ActualRaDec;
            ln_get_equ_from_hrz(&ActualAltAz, &Position, ln_get_julian_from_sys(), &ActualRaDec);
            RightAscension = ActualRaDec.ra;
            Declination = ActualRaDec.dec;
            gsl_vector_free(pGSLActualVector);
            gsl_vector_free(pGSLApparentVector);
            break;
        }

        default:
            // Convex hull stuff to go in here
            return false;
    }
    return true;
}

// Private methods

void  BuiltInMathPlugin::CalculateTAKIMatrices(const TelescopeDirectionVector& Actual1, const TelescopeDirectionVector& Actual2, const TelescopeDirectionVector& Actual3,
                            const TelescopeDirectionVector& Apparent1, const TelescopeDirectionVector& Apparent2, const TelescopeDirectionVector& Apparent3,
                            gsl_matrix *pActualToApparent, gsl_matrix *pApparentToActual)
{
    // Derive the Actual to Apparent transformation matrix using TAKI's method
    gsl_matrix *pActualMatrix = gsl_matrix_alloc(3, 3);
    gsl_matrix_set(pActualMatrix, 0, 0, Actual1.x);
    gsl_matrix_set(pActualMatrix, 0, 1, Actual1.y);
    gsl_matrix_set(pActualMatrix, 0, 2, Actual1.z);
    gsl_matrix_set(pActualMatrix, 1, 0, Actual2.x);
    gsl_matrix_set(pActualMatrix, 1, 1, Actual2.y);
    gsl_matrix_set(pActualMatrix, 1, 2, Actual2.z);
    gsl_matrix_set(pActualMatrix, 2, 0, Actual3.x);
    gsl_matrix_set(pActualMatrix, 2, 1, Actual3.y);
    gsl_matrix_set(pActualMatrix, 2, 2, Actual3.z);

    gsl_matrix *pApparentMatrix = gsl_matrix_alloc(3, 3);
    gsl_matrix_set(pApparentMatrix, 0, 0, Apparent1.x);
    gsl_matrix_set(pApparentMatrix, 0, 1, Apparent1.y);
    gsl_matrix_set(pApparentMatrix, 0, 2, Apparent1.z);
    gsl_matrix_set(pApparentMatrix, 1, 0, Apparent2.x);
    gsl_matrix_set(pApparentMatrix, 1, 1, Apparent2.y);
    gsl_matrix_set(pApparentMatrix, 1, 2, Apparent2.z);
    gsl_matrix_set(pApparentMatrix, 2, 0, Apparent3.x);
    gsl_matrix_set(pApparentMatrix, 2, 1, Apparent3.y);
    gsl_matrix_set(pApparentMatrix, 2, 2, Apparent3.z);

    MatrixMatrixMultipy(pApparentMatrix, pActualMatrix, pActualToApparent);

    // Use pApparent as temporary storage
    gsl_matrix_memcpy(pApparentMatrix, pActualToApparent);

    // Invert the matrix to get the Apparent to Actual transform
    MatrixInvert3x3(pActualToApparent, pApparentToActual);
    // Clean up
    gsl_matrix_free(pApparentMatrix);
    gsl_matrix_free(pActualMatrix);
}

/// Use gsl to compute the inverse of a 3x3 matrix
void BuiltInMathPlugin::MatrixInvert3x3(gsl_matrix *pInput, gsl_matrix *pInversion)
{
    gsl_permutation *pPermutation = gsl_permutation_alloc(3);
    gsl_matrix *pDecomp = gsl_matrix_alloc(3,3);
    int Signum;

    gsl_matrix_memcpy(pDecomp, pInput);
    gsl_linalg_LU_decomp(pDecomp, pPermutation, &Signum);
    gsl_linalg_LU_invert(pDecomp, pPermutation, pInversion);

    gsl_matrix_free(pDecomp);
    gsl_permutation_free(pPermutation);
}

/// Use gsl blas support to multiply two matrices together and put the result in a third.
/// For our purposes all the matrices should be 3 by 3.
void BuiltInMathPlugin::MatrixMatrixMultipy(gsl_matrix *pA, gsl_matrix *pB, gsl_matrix *pC)
{
    // Zeroise the output matrix
    gsl_matrix_set_zero(pC);

    gsl_blas_dgemm(CblasNoTrans, CblasNoTrans, 1.0, pA, pB, 0.0, pC);
}

/// Use gsl blas support to multiply a matrix by a vector and put the result in another vector
/// For our purposes the the matrix should be 3x3 and vector 3.
void BuiltInMathPlugin::MatrixVectorMultipy(gsl_matrix *pA, gsl_vector *pB, gsl_vector *pC)
{
    // Zeroise the output matrix
    gsl_vector_set_zero(pC);

    gsl_blas_dgemv(CblasNoTrans, 1.0, pA, pB, 0.0, pC);
}



} // namespace AlignmentSubsystem
} // namespace INDI