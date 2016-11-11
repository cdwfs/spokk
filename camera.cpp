#include "camera.h"

#define _USE_MATH_DEFINES
#include <math.h>

#include <algorithm>

using namespace mathfu;

namespace {
    const vec3 kForward(0, 0, -1);
    const vec3 kUp(0, 1, 0);
    const vec3 kRight(1, 0, 0);

    float toDegrees(float radians) {
        return radians * 180.0f / float(M_PI);
    }

    float toRadians(float degrees) {
        return degrees * float(M_PI) / 180.0f;
    }

    float distance(const vec3& v1, const vec3 &v2) {
        return (v1-v2).Length();
    }
}  // namespace

void Camera::setEyePoint( const vec3 &eyePoint )
{
    mEyePoint = eyePoint;
    mModelViewCached = false;
}

void Camera::setViewDirection( const vec3 &viewDirection )
{
    mViewDirection = normalize( viewDirection );
    mOrientation = quat::RotateFromTo(kForward, mViewDirection);
    mModelViewCached = false;
}

void Camera::setOrientation( const quat &orientation )
{
    mOrientation = normalize( orientation );
    mViewDirection = mOrientation * kForward;
    mModelViewCached = false;
}

float Camera::getFovHorizontal() const
{
    return toDegrees( 2.0f * atanf( tanf( toRadians(mFov) * 0.5f ) * mAspectRatio ) );
}

void Camera::setFovHorizontal( float horizontalFov )
{
    mFov = toDegrees( 2.0f * atanf( tanf( toRadians( horizontalFov ) * 0.5f ) / mAspectRatio ) );
    mProjectionCached = false;
}

// Derived from math presented in http://paulbourke.net/miscellaneous/lens/
float Camera::getFocalLength() const
{
    return 1.0f / ( tanf( toRadians( mFov ) * 0.5f ) * 2.0f );
}

void Camera::setWorldUp( const vec3 &worldUp )
{
    mWorldUp = normalize( worldUp );
    mOrientation = quat::RotateFromToWithAxis(kForward, mViewDirection, mWorldUp);
    mModelViewCached = false;
}

void Camera::lookAt( const vec3 &target )
{
    mViewDirection = normalize( target - mEyePoint );
    mOrientation = quat::RotateFromToWithAxis(kForward, mViewDirection, mWorldUp);
    mPivotDistance = distance( target, mEyePoint );
    mModelViewCached = false;
}

void Camera::lookAt( const vec3 &eyePoint, const vec3 &target )
{
    mEyePoint = eyePoint;
    mViewDirection = normalize( target - mEyePoint );
    mOrientation = quat::RotateFromToWithAxis(kForward, mViewDirection, mWorldUp);
    mPivotDistance = distance( target, mEyePoint );
    mModelViewCached = false;
}

void Camera::lookAt( const vec3 &eyePoint, const vec3 &target, const vec3 &aWorldUp )
{
    mEyePoint = eyePoint;
    mWorldUp = normalize( aWorldUp );
    mViewDirection = normalize( target - mEyePoint );
    mOrientation = quat::RotateFromToWithAxis(kForward, mViewDirection, mWorldUp);
    mPivotDistance = distance( target, mEyePoint );
    mModelViewCached = false;
}

void Camera::getNearClipCoordinates( vec3 *topLeft, vec3 *topRight, vec3 *bottomLeft, vec3 *bottomRight ) const
{
    calcMatrices();

    vec3 viewDirection = normalize( mViewDirection );

    *topLeft		= mEyePoint + (mNearClip * viewDirection) + (mFrustumTop * mV) + (mFrustumLeft * mU);
    *topRight		= mEyePoint + (mNearClip * viewDirection) + (mFrustumTop * mV) + (mFrustumRight * mU);
    *bottomLeft		= mEyePoint + (mNearClip * viewDirection) + (mFrustumBottom * mV) + (mFrustumLeft * mU);
    *bottomRight	= mEyePoint + (mNearClip * viewDirection) + (mFrustumBottom * mV) + (mFrustumRight * mU);
}

void Camera::getFarClipCoordinates( vec3 *topLeft, vec3 *topRight, vec3 *bottomLeft, vec3 *bottomRight ) const
{
    calcMatrices();

    vec3 viewDirection = normalize( mViewDirection );
    float ratio = mFarClip / mNearClip;

    *topLeft		= mEyePoint + (mFarClip * viewDirection) + (ratio * mFrustumTop * mV) + (ratio * mFrustumLeft * mU);
    *topRight		= mEyePoint + (mFarClip * viewDirection) + (ratio * mFrustumTop * mV) + (ratio * mFrustumRight * mU);
    *bottomLeft		= mEyePoint + (mFarClip * viewDirection) + (ratio * mFrustumBottom * mV) + (ratio * mFrustumLeft * mU);
    *bottomRight	= mEyePoint + (mFarClip * viewDirection) + (ratio * mFrustumBottom * mV) + (ratio * mFrustumRight * mU);
}

void Camera::getFrustum( float *left, float *top, float *right, float *bottom, float *near, float *far ) const
{
    calcMatrices();

    *left = mFrustumLeft;
    *top = mFrustumTop;
    *right = mFrustumRight;
    *bottom = mFrustumBottom;
    *near = mNearClip;
    *far = mFarClip;
}

void Camera::getBillboardVectors( vec3 *right, vec3 *up ) const
{
    // TODO(cort): original code used row, not column...
    *right = getViewMatrix().GetColumn(0).xyz();
    *up = getViewMatrix().GetColumn(1).xyz();
}

vec2 Camera::worldToScreen( const vec3 &worldCoord, float screenWidth, float screenHeight ) const
{
    vec4 eyeCoord = getViewMatrix() * vec4( worldCoord, 1 );
    vec4 ndc = getProjectionMatrix() * eyeCoord;
    ndc[0] /= ndc.w();
    ndc[1] /= ndc.w();
    //ndc[2] /= ndc.w();

    return vec2( ( ndc.x() + 1.0f ) / 2.0f * screenWidth, ( 1.0f - ( ndc.y() + 1.0f ) / 2.0f ) * screenHeight );
}

vec2 Camera::eyeToScreen( const vec3 &eyeCoord, const vec2 &screenSizePixels ) const
{
    vec4 ndc = getProjectionMatrix() * vec4( eyeCoord, 1 );
    ndc[0] /= ndc.w();
    ndc[1] /= ndc.w();
    //ndc[2] /= ndc.w();

    return vec2( ( ndc.x() + 1.0f ) / 2.0f * screenSizePixels.x(), ( 1.0f - ( ndc.y() + 1.0f ) / 2.0f ) * screenSizePixels.y() );
}

float Camera::worldToEyeDepth( const vec3 &worldCoord ) const
{
    const mat4 &m = getViewMatrix();
    return	m(2,0) * worldCoord.x() +
            m(2,1) * worldCoord.y() +
            m(2,2) * worldCoord.z() +
            m(2,3);
}


vec3 Camera::worldToNdc( const vec3 &worldCoord ) const
{
    vec4 eye = getViewMatrix() * vec4( worldCoord, 1 );
    vec4 unproj = getProjectionMatrix() * eye;
    return unproj.xyz() / unproj.w();
}

/*
float Camera::calcScreenArea( const Sphere &sphere, const vec2 &screenSizePixels ) const
{
    Sphere camSpaceSphere( vec3( getViewMatrix()*vec4(sphere.getCenter(), 1.0f) ), sphere.getRadius() );
    return camSpaceSphere.calcProjectedArea( getFocalLength(), screenSizePixels );
}

void Camera::calcScreenProjection( const Sphere &sphere, const vec2 &screenSizePixels, vec2 *outCenter, vec2 *outAxisA, vec2 *outAxisB ) const
{
    auto toScreenPixels = [=] ( vec2 v, const vec2 &windowSize ) {
        vec2 result = v;
        result.x *= 1 / ( windowSize.x / windowSize.y );
        result += vec2( 0.5f );
        result *= windowSize;
        return result;
    };

    Sphere camSpaceSphere( vec3( getViewMatrix()*vec4(sphere.getCenter(), 1.0f) ), sphere.getRadius() );
    vec2 center, axisA, axisB;
    camSpaceSphere.calcProjection( getFocalLength(), &center, &axisA, &axisB );
    if( outCenter )
        *outCenter = toScreenPixels( center, screenSizePixels );//( center * vec2( invAspectRatio, 1 ) + vec2( 0.5f ) ) * screenSizePixels;
    if( outAxisA )
        *outAxisA = toScreenPixels( center + axisA * 0.5f, screenSizePixels ) - toScreenPixels( center - axisA * 0.5f, screenSizePixels );
    if( outAxisB )
        *outAxisB = toScreenPixels( center + axisB * 0.5f, screenSizePixels ) - toScreenPixels( center - axisB * 0.5f, screenSizePixels );
}
*/

void Camera::calcMatrices() const
{
    if( ! mModelViewCached ) calcViewMatrix();
    if( ! mProjectionCached ) calcProjection();
}

void Camera::calcViewMatrix() const
{
    mW = - normalize( mViewDirection );
    mU = mOrientation * kRight;
    mV = mOrientation * kUp;

    vec3 d( - dot( mEyePoint, mU ), - dot( mEyePoint, mV ), - dot( mEyePoint, mW ) );

    mat4 &m = mViewMatrix;
    m(0,0) = mU.x(); m(1,0) = mV.x(); m(2,0) = mW.x(); m(3,0) = 0.0f;
    m(0,1) = mU.y(); m(1,1) = mV.y(); m(2,1) = mW.y(); m(3,1) = 0.0f;
    m(0,2) = mU.z(); m(1,2) = mV.z(); m(2,2) = mW.z(); m(3,2) = 0.0f;
    m(0,3) =  d.x(); m(1,3) =  d.y(); m(2,3) =  d.z(); m(3,3) = 1.0f;

    mModelViewCached = true;
    mInverseModelViewCached = false;
}

void Camera::calcInverseView() const
{
    if( ! mModelViewCached ) calcViewMatrix();

    mInverseModelViewMatrix = mViewMatrix.Inverse();
    mInverseModelViewCached = true;
}

/*
Ray Camera::calcRay( float uPos, float vPos, float imagePlaneApectRatio ) const
{
    calcMatrices();

    float s = ( uPos - 0.5f ) * imagePlaneApectRatio;
    float t = ( vPos - 0.5f );
    float viewDistance = imagePlaneApectRatio / math<float>::abs( mFrustumRight - mFrustumLeft ) * mNearClip;
    return Ray( mEyePoint, normalize( mU * s + mV * t - ( mW * viewDistance ) ) );
}
*/

////////////////////////////////////////////////////////////////////////////////////////
// CameraPersp
// Creates a default camera resembling Maya Persp
CameraPersp::CameraPersp()
    : Camera()
{
    lookAt( vec3( 28, 21, 28 ), vec3(), vec3( 0, 1, 0 ) );
    setPerspective( 35, 1.3333f, 0.1f, 1000 );
    setLensShift(0,0);
}

CameraPersp::CameraPersp( int pixelWidth, int pixelHeight, float fovDegrees )
    : Camera()
{
    float eyeX          = pixelWidth / 2.0f;
    float eyeY          = pixelHeight / 2.0f;
    float halfFov   = 3.14159f * fovDegrees / 360.0f;
    float theTan    = tanf( halfFov );
    float dist          = eyeY / theTan;
    float nearDist      = dist / 10.0f;	// near / far clip plane
    float farDist   = dist * 10.0f;
    float aspect    = pixelWidth / (float)pixelHeight;

    setPerspective( fovDegrees, aspect, nearDist, farDist );
    lookAt( vec3( eyeX, eyeY, dist ), vec3( eyeX, eyeY, 0.0f ) );
    setLensShift(0,0);
}

CameraPersp::CameraPersp( int pixelWidth, int pixelHeight, float fovDegrees, float nearPlane, float farPlane )
    : Camera()
{
    float halfFov, theTan, aspect;

    float eyeX          = pixelWidth / 2.0f;
    float eyeY          = pixelHeight / 2.0f;
    halfFov         = 3.14159f * fovDegrees / 360.0f;
    theTan              = tanf( halfFov );
    float dist          = eyeY / theTan;
    aspect              = pixelWidth / (float)pixelHeight;

    setPerspective( fovDegrees, aspect, nearPlane, farPlane );
    lookAt( vec3( eyeX, eyeY, dist ), vec3( eyeX, eyeY, 0.0f ) );
    setLensShift(0,0);
}

void CameraPersp::setPerspective( float verticalFovDegrees, float aspectRatio, float nearPlane, float farPlane )
{
    mFov			= verticalFovDegrees;
    mAspectRatio	= aspectRatio;
    mNearClip		= nearPlane;
    mFarClip		= farPlane;

    mProjectionCached = false;
}

/*
Ray CameraPersp::calcRay( float uPos, float vPos, float imagePlaneApectRatio ) const
{
    calcMatrices();

    float s = ( uPos - 0.5f + 0.5f * mLensShift.x ) * imagePlaneApectRatio;
    float t = ( vPos - 0.5f + 0.5f * mLensShift.y );
    float viewDistance = imagePlaneApectRatio / math<float>::abs( mFrustumRight - mFrustumLeft ) * mNearClip;
    return Ray( mEyePoint, normalize( mU * s + mV * t - ( mW * viewDistance ) ) );
}
*/

void CameraPersp::calcProjection() const
{
    mFrustumTop		=  mNearClip * tanf( toRadians(mFov) * 0.5f );
    mFrustumBottom	= -mFrustumTop;
    mFrustumRight	=  mFrustumTop * mAspectRatio;
    mFrustumLeft	= -mFrustumRight;

    // perform lens shift
    if( mLensShift.y() != 0.0f ) {
        mFrustumTop = Lerp(0.0f, 2.0f * mFrustumTop, 0.5f + 0.5f * mLensShift.y());
        mFrustumBottom = Lerp(2.0f * mFrustumBottom, 0.0f, 0.5f + 0.5f * mLensShift.y());
    }

    if( mLensShift.x() != 0.0f ) {
        mFrustumRight = Lerp(2.0f * mFrustumRight, 0.0f, 0.5f - 0.5f * mLensShift.x());
        mFrustumLeft = Lerp(0.0f, 2.0f * mFrustumLeft, 0.5f - 0.5f * mLensShift.x());
    }

    mat4 &p = mProjectionMatrix;
    p(0,0) =  2.0f * mNearClip / ( mFrustumRight - mFrustumLeft );
    p(0,1) =  0.0f;
    p(0,2) =  ( mFrustumRight + mFrustumLeft ) / ( mFrustumRight - mFrustumLeft );
    p(0,3) =  0.0f;

    p(1,0) =  0.0f;
    p(1,1) =  2.0f * mNearClip / ( mFrustumTop - mFrustumBottom );
    p(1,2) =  ( mFrustumTop + mFrustumBottom ) / ( mFrustumTop - mFrustumBottom );
    p(1,3) =  0.0f;

    p(2,0) =  0.0f;
    p(2,1) =  0.0f;
    p(2,2) = -( mFarClip + mNearClip ) / ( mFarClip - mNearClip );
    p(2,3) = -2.0f * mFarClip * mNearClip / ( mFarClip - mNearClip );

    p(3,0) =  0.0f;
    p(3,1) =  0.0f;
    p(3,2) = -1.0f;
    p(3,3) =  0.0f;

    mat4 &m = mInverseProjectionMatrix;
    m(0,0) =  ( mFrustumRight - mFrustumLeft ) / ( 2.0f * mNearClip );
    m(0,1) =  0.0f;
    m(0,2) =  0.0f;
    m(0,3) =  ( mFrustumRight + mFrustumLeft ) / ( 2.0f * mNearClip );

    m(1,0) =  0.0f;
    m(1,1) =  ( mFrustumTop - mFrustumBottom ) / ( 2.0f * mNearClip );
    m(1,2) =  0.0f;
    m(1,3) =  ( mFrustumTop + mFrustumBottom ) / ( 2.0f * mNearClip );

    m(2,0) =  0.0f;
    m(2,1) =  0.0f;
    m(2,2) =  0.0f;
    m(2,3) = -1.0f;

    m(3,0) =  0.0f;
    m(3,1) =  0.0f;
    m(3,2) = -( mFarClip - mNearClip ) / ( 2.0f * mFarClip*mNearClip );
    m(3,3) =  ( mFarClip + mNearClip ) / ( 2.0f * mFarClip*mNearClip );

    mProjectionCached = true;
}

void CameraPersp::setLensShift(float horizontal, float vertical)
{
    mLensShift = vec2(horizontal, vertical);

    mProjectionCached = false;
}

/*
CameraPersp	CameraPersp::calcFraming( const Sphere &worldSpaceSphere ) const
{
    CameraPersp result = *this;
    float xDistance = worldSpaceSphere.getRadius() / sin( toRadians( getFovHorizontal() * 0.5f ) );
    float yDistance = worldSpaceSphere.getRadius() / sin( toRadians( getFov() * 0.5f ) );
    result.setEyePoint( worldSpaceSphere.getCenter() - result.mViewDirection * std::max( xDistance, yDistance ) );
    result.mPivotDistance = distance( result.mEyePoint, worldSpaceSphere.getCenter() );
    return result;
}
*/

////////////////////////////////////////////////////////////////////////////////////////
// CameraOrtho
CameraOrtho::CameraOrtho()
    : Camera()
{
    lookAt( vec3( 0, 0, 0.1f ), vec3(), vec3( 0, 1, 0 ) );
    setFov( 35 );
}

CameraOrtho::CameraOrtho( float left, float right, float bottom, float top, float nearPlane, float farPlane )
    : Camera()
{
    mFrustumLeft	= left;
    mFrustumRight	= right;
    mFrustumTop		= top;
    mFrustumBottom	= bottom;
    mNearClip		= nearPlane;
    mFarClip		= farPlane;

    mProjectionCached = false;
    mModelViewCached = true;
    mInverseModelViewCached = true;
}

void CameraOrtho::setOrtho( float left, float right, float bottom, float top, float nearPlane, float farPlane )
{
    mFrustumLeft	= left;
    mFrustumRight	= right;
    mFrustumTop		= top;
    mFrustumBottom	= bottom;
    mNearClip		= nearPlane;
    mFarClip		= farPlane;

    mProjectionCached = false;
}

void CameraOrtho::calcProjection() const
{
    mat4 &p = mProjectionMatrix;
    p(0,0) =  2 / (mFrustumRight - mFrustumLeft);
    p(0,1) =  0;
    p(0,2) =  0;
    p(0,3) =  -(mFrustumRight + mFrustumLeft) / (mFrustumRight - mFrustumLeft);

    p(1,0) =  0;
    p(1,1) =  2 / (mFrustumTop - mFrustumBottom);
    p(1,2) =  0;
    p(1,3) =  -(mFrustumTop + mFrustumBottom) / (mFrustumTop - mFrustumBottom);

    p(2,0) =  0;
    p(2,1) =  0;
    p(2,2) = -2 / (mFarClip - mNearClip);
    p(2,3) = -(mFarClip + mNearClip) / (mFarClip - mNearClip);

    p(3,0) =  0;
    p(3,1) =  0;
    p(3,2) =  0;
    p(3,3) =  1;

    mat4 &m = mInverseProjectionMatrix;
    m(0,0) =  (mFrustumRight - mFrustumLeft) * 0.5f;
    m(0,1) =  0;
    m(0,2) =  0;
    m(0,3) =  (mFrustumRight + mFrustumLeft) * 0.5f;

    m(1,0) =  0;
    m(1,1) =  (mFrustumTop - mFrustumBottom) * 0.5f;
    m(1,2) =  0;
    m(1,3) =  (mFrustumTop + mFrustumBottom) * 0.5f;

    m(2,0) =  0;
    m(2,1) =  0;
    m(2,2) =  (mFarClip - mNearClip) * 0.5f;
    m(2,3) =  (mNearClip + mFarClip) * 0.5f;

    m(3,0) =  0;
    m(3,1) =  0;
    m(3,2) =  0;
    m(3,3) =  1;

    mProjectionCached = true;
}

////////////////////////////////////////////////////////////////////////////////////////
// CameraStereo
vec3 CameraStereo::getEyePointShifted() const
{
    if( ! mIsStereo )
        return mEyePoint;

    if( mIsLeft )
        return mEyePoint - ( mOrientation * kRight ) * ( 0.5f * mEyeSeparation );
    else
        return mEyePoint + ( mOrientation * kRight ) * ( 0.5f * mEyeSeparation );
}

void CameraStereo::setConvergence( float distance, bool adjustEyeSeparation )
{
    mConvergence = distance; mProjectionCached = false;

    if( adjustEyeSeparation )
        mEyeSeparation = mConvergence / 30.0f;
}

void CameraStereo::getNearClipCoordinates( vec3 *topLeft, vec3 *topRight, vec3 *bottomLeft, vec3 *bottomRight ) const
{
    calcMatrices();

    vec3 viewDirection = normalize( mViewDirection );

    vec3 eye( getEyePointShifted() );

    float shift = 0.5f * mEyeSeparation * (mNearClip / mConvergence);
    shift *= (mIsStereo ? (mIsLeft ? 1.0f : -1.0f) : 0.0f);

    float left = mFrustumLeft + shift;
    float right = mFrustumRight + shift;

    *topLeft		= eye + (mNearClip * viewDirection) + (mFrustumTop * mV) + (left * mU);
    *topRight		= eye + (mNearClip * viewDirection) + (mFrustumTop * mV) + (right * mU);
    *bottomLeft		= eye + (mNearClip * viewDirection) + (mFrustumBottom * mV) + (left * mU);
    *bottomRight	= eye + (mNearClip * viewDirection) + (mFrustumBottom * mV) + (right * mU);
}

void CameraStereo::getFarClipCoordinates( vec3 *topLeft, vec3 *topRight, vec3 *bottomLeft, vec3 *bottomRight ) const
{
    calcMatrices();

    vec3 viewDirection = normalize( mViewDirection );
    float ratio = mFarClip / mNearClip;

    vec3 eye( getEyePointShifted() );

    float shift = 0.5f * mEyeSeparation * (mNearClip / mConvergence);
    shift *= (mIsStereo ? (mIsLeft ? 1.0f : -1.0f) : 0.0f);

    float left = mFrustumLeft + shift;
    float right = mFrustumRight + shift;

    *topLeft		= eye + (mFarClip * viewDirection) + (ratio * mFrustumTop * mV) + (ratio * left * mU);
    *topRight		= eye + (mFarClip * viewDirection) + (ratio * mFrustumTop * mV) + (ratio * right * mU);
    *bottomLeft		= eye + (mFarClip * viewDirection) + (ratio * mFrustumBottom * mV) + (ratio * left * mU);
    *bottomRight	= eye + (mFarClip * viewDirection) + (ratio * mFrustumBottom * mV) + (ratio * right * mU);
}

const mat4& CameraStereo::getProjectionMatrix() const
{
    if( ! mProjectionCached )
        calcProjection();

    if( ! mIsStereo )
        return mProjectionMatrix;
    else if( mIsLeft )
        return mProjectionMatrixLeft;
    else
        return mProjectionMatrixRight;
}

const mat4& CameraStereo::getViewMatrix() const
{
    if( ! mModelViewCached )
        calcViewMatrix();

    if( ! mIsStereo )
        return mViewMatrix;
    else if( mIsLeft )
        return mViewMatrixLeft;
    else
        return mViewMatrixRight;
}

const mat4& CameraStereo::getInverseViewMatrix() const
{
    if( ! mInverseModelViewCached )
        calcInverseView();

    if( ! mIsStereo )
        return mInverseModelViewMatrix;
    else if( mIsLeft )
        return mInverseModelViewMatrixLeft;
    else
        return mInverseModelViewMatrixRight;
}

void CameraStereo::calcViewMatrix() const
{
    // calculate default matrix first
    CameraPersp::calcViewMatrix();

    mViewMatrixLeft = mViewMatrix;
    mViewMatrixRight = mViewMatrix;

    // calculate left matrix
    vec3 eye = mEyePoint - ( mOrientation * kRight ) * ( 0.5f * mEyeSeparation );
    vec3 d = vec3( - dot( eye, mU ), - dot( eye, mV ), - dot( eye, mW ) );

    mViewMatrixLeft(0,3) = d.x();
    mViewMatrixLeft(1,3) = d.y();
    mViewMatrixLeft(2,3) = d.z();

    // calculate right matrix
    eye = mEyePoint + ( mOrientation * kRight ) * ( 0.5f * mEyeSeparation );
    d = vec3( - dot( eye, mU ), - dot( eye, mV ), - dot( eye, mW ) );

    mViewMatrixRight(0,3) = d.x();
    mViewMatrixRight(1,3) = d.y();
    mViewMatrixRight(2,3) = d.z();

    mModelViewCached = true;
    mInverseModelViewCached = false;
}

void CameraStereo::calcInverseView() const
{
    if( ! mModelViewCached ) calcViewMatrix();

    mInverseModelViewMatrix = mViewMatrix.Inverse();
    mInverseModelViewMatrixLeft = mViewMatrixLeft.Inverse();
    mInverseModelViewMatrixRight = mViewMatrixRight.Inverse();
    mInverseModelViewCached = true;
}

void CameraStereo::calcProjection() const
{
    // calculate default matrices first
    CameraPersp::calcProjection();

    mProjectionMatrixLeft = mProjectionMatrix;
    mInverseProjectionMatrixLeft = mInverseProjectionMatrix;

    mProjectionMatrixRight = mProjectionMatrix;
    mInverseProjectionMatrixRight = mInverseProjectionMatrix;

    // calculate left matrices
    mInverseProjectionMatrixLeft(0,2) =  ( mFrustumRight + mFrustumLeft + mEyeSeparation * (mNearClip / mConvergence) ) / ( mFrustumRight - mFrustumLeft );

    mInverseProjectionMatrixLeft(0,3) =  ( mFrustumRight + mFrustumLeft + mEyeSeparation * (mNearClip / mConvergence) ) / ( 2.0f * mNearClip );

    // calculate right matrices
    mProjectionMatrixRight(0,2) =  ( mFrustumRight + mFrustumLeft - mEyeSeparation * (mNearClip / mConvergence) ) / ( mFrustumRight - mFrustumLeft );

    mProjectionMatrixRight(0,3) =  ( mFrustumRight + mFrustumLeft - mEyeSeparation * (mNearClip / mConvergence) ) / ( 2.0f * mNearClip );

    mProjectionCached = true;
}
