#if !defined(CAMERA_H)
#define CAMERA_H

#include <glm/mat4x4.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>
#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable:4701)  // potentially uninitialized return value
#endif
#include <glm/gtc/quaternion.hpp>
#if defined(_MSC_VER)
#pragma warning(pop)
#endif

#include <float.h>

class Camera {
  public:
    virtual ~Camera() {}
    Camera& operator=(const Camera &rhs) = delete;

    //! Returns the position in world-space from which the Camera is viewing
    glm::vec3		getEyePoint() const { return mEyePoint; }
    //! Sets the position in world-space from which the Camera is viewing
    void		setEyePoint( const glm::vec3 &eyePoint );

    //! Returns the vector in world-space which represents "up" - typically glm::vec3( 0, 1, 0 )
    glm::vec3		getWorldUp() const { return mWorldUp; }
    //! Sets the vector in world-space which represents "up" - typically glm::vec3( 0, 1, 0 )
    void		setWorldUp( const glm::vec3 &worldUp );

    //! Modifies the view direction to look from the current eyePoint to \a target. Also updates the pivot distance.
    void		lookAt( const glm::vec3 &target );
    //! Modifies the eyePoint and view direction to look from \a eyePoint to \a target. Also updates the pivot distance.
    void		lookAt( const glm::vec3 &eyePoint, const glm::vec3 &target );
    //! Modifies the eyePoint and view direction to look from \a eyePoint to \a target with up vector \a up (to achieve camera roll). Also updates the pivot distance.
    void		lookAt( const glm::vec3 &eyePoint, const glm::vec3 &target, const glm::vec3 &up );
    //! Returns the world-space vector along which the camera is oriented
    glm::vec3		getViewDirection() const { return mViewDirection; }
    //! Sets the world-space vector along which the camera is oriented
    void		setViewDirection( const glm::vec3 &viewDirection );

    //! Returns the world-space quaternion that expresses the camera's orientation
    glm::quat		getOrientation() const { return mOrientation; }
    //! Returns the world-space Euler angles in Yaw, Pitch, Roll order with +Y=up, -Z=forward.
    glm::vec3    getEulersYPR() const;
    //! Sets the camera's orientation with world-space quaternion \a orientation
    void		setOrientation( const glm::quat &orientation );

    //! Returns the camera's vertical field of view measured in degrees.
    float	getFov() const { return mFov; }
    //! Sets the camera's vertical field of view measured in degrees.
    void	setFov( float verticalFov ) { mFov = verticalFov;  mProjectionCached = false; }
    //! Returns the camera's horizontal field of view measured in degrees.
    float	getFovHorizontal() const;
    //! Sets the camera's horizontal field of view measured in degrees.
    void	setFovHorizontal( float horizontalFov );
    //! Returns the camera's focal length, calculating it based on the field of view.
    float	getFocalLength() const;

    //! Primarily for user interaction, such as with CameraUi. Returns the distance from the camera along the view direction relative to which tumbling and dollying occur.
    float	getPivotDistance() const { return mPivotDistance; }
    //! Primarily for user interaction, such as with CameraUi. Sets the distance from the camera along the view direction relative to which tumbling and dollying occur.
    void	setPivotDistance( float distance ) { mPivotDistance = distance; }
    //! Primarily for user interaction, such as with CameraUi. Returns the world-space point relative to which tumbling and dollying occur.
    glm::vec3	getPivotPoint() const { return mEyePoint + mViewDirection * mPivotDistance; }

    //! Returns the aspect ratio of the image plane - its width divided by its height
    float	getAspectRatio() const { return mAspectRatio; }
    //! Sets the aspect ratio of the image plane - its width divided by its height
    void	setAspectRatio( float aAspectRatio ) { mAspectRatio = aAspectRatio; mProjectionCached = false; }
    //! Returns the distance along the view direction to the Near clipping plane.
    float	getNearClip() const { return mNearClip; }
    //! Sets the distance along the view direction to the Near clipping plane.
    void	setNearClip( float nearClip ) { mNearClip = nearClip; mProjectionCached = false; }
    //! Returns the distance along the view direction to the Far clipping plane.
    float	getFarClip() const { return mFarClip; }
    //! Sets the distance along the view direction to the Far clipping plane.
    void	setFarClip( float farClip ) { mFarClip = farClip; mProjectionCached = false; }

    //! Returns the four corners of the Camera's Near clipping plane, expressed in world-space
    virtual void	getNearClipCoordinates( glm::vec3 *topLeft, glm::vec3 *topRight, glm::vec3 *bottomLeft, glm::vec3 *bottomRight ) const;
    //! Returns the four corners of the Camera's Far clipping plane, expressed in world-space
    virtual void	getFarClipCoordinates( glm::vec3 *topLeft, glm::vec3 *topRight, glm::vec3 *bottomLeft, glm::vec3 *bottomRight ) const;

    //! Returns the coordinates of the camera's frustum, suitable for passing to \c glFrustum
    void	getFrustum( float *left, float *top, float *right, float *bottom, float *near, float *far ) const;
    //! Returns whether the camera represents a perspective projection instead of an orthographic
    virtual bool isPersp() const = 0;

    //! Returns the Camera's Projection matrix, which converts view-space into clip-space
    virtual const glm::mat4&	getProjectionMatrix() const { if( ! mProjectionCached ) calcProjection(); return mProjectionMatrix; }
    //! Returns the Camera's View matrix, which converts world-space into view-space
    virtual const glm::mat4&	getViewMatrix() const { if( ! mModelViewCached ) calcViewMatrix(); return mViewMatrix; }
    //! Returns the Camera's Inverse View matrix, which converts view-space into world-space
    virtual const glm::mat4&	getInverseViewMatrix() const { if( ! mInverseModelViewCached ) calcInverseView(); return mInverseModelViewMatrix; }

    //! Returns a Ray that passes through the image plane coordinates (\a u, \a v) (expressed in the range [0,1]) on an image plane of aspect ratio \a imagePlaneAspectRatio
//	Ray		generateRay( float u, float v, float imagePlaneAspectRatio ) const { return calcRay( u, v, imagePlaneAspectRatio ); }
    //! Returns a Ray that passes through the pixels coordinates \a posPixels on an image of size \a imageSizePixels
//	Ray		generateRay( const glm::vec2 &posPixels, const glm::vec2 &imageSizePixels ) const { return calcRay( posPixels.x / imageSizePixels.x, ( imageSizePixels.y - posPixels.y ) / imageSizePixels.y, imageSizePixels.x / imageSizePixels.y ); }
    //! Returns the \a right and \a up vectors suitable for billboarding relative to the Camera
    void	getBillboardVectors( glm::vec3 *right, glm::vec3 *up ) const;

    //! Converts a world-space coordinate \a worldCoord to screen coordinates as viewed by the camera, based on a screen which is \a screenWidth x \a screenHeight pixels.
    glm::vec2 worldToScreen( const glm::vec3 &worldCoord, float screenWidth, float screenHeight ) const;
    //! Converts a eye-space coordinate \a eyeCoord to screen coordinates as viewed by the camera
    glm::vec2 eyeToScreen( const glm::vec3 &eyeCoord, const glm::vec2 &screenSizePixels ) const;
    //! Converts a world-space coordinate \a worldCoord to eye-space, also known as camera-space. -Z is along the view direction.
    glm::vec3 worldToEye( const glm::vec3 &worldCoord ) const	{ return glm::vec3((getViewMatrix() * glm::vec4( worldCoord, 1 ))); }
    //! Converts a world-space coordinate \a worldCoord to the z axis of eye-space, also known as camera-space. -Z is along the view direction. Suitable for depth sorting.
    float worldToEyeDepth( const glm::vec3 &worldCoord ) const;
    //! Converts a world-space coordinate \a worldCoord to normalized device coordinates
    glm::vec3 worldToNdc( const glm::vec3 &worldCoord ) const;

    //! Calculates the area of the screen-space elliptical projection of \a sphere
//	float	calcScreenArea( const Sphere &sphere, const glm::vec2 &screenSizePixels ) const;
    //! Calculates the screen-space elliptical projection of \a sphere, putting the results in \a outCenter, \a outAxisA and \a outAxisB
//	void	calcScreenProjection( const Sphere &sphere, const glm::vec2 &screenSizePixels, glm::vec2 *outCenter, glm::vec2 *outAxisA, glm::vec2 *outAxisB ) const;

  protected:
    Camera()
        : mWorldUp( glm::vec3( 0, 1, 0 ) ), mPivotDistance( 0 ), mProjectionCached( false ), mModelViewCached( false ), mInverseModelViewCached( false )
    {}

    void			calcMatrices() const;

    virtual void	calcViewMatrix() const;
    virtual void	calcInverseView() const;
    virtual void	calcProjection() const = 0;

//	virtual Ray		calcRay( float u, float v, float imagePlaneAspectRatio ) const;

    glm::vec3	mEyePoint;
    glm::vec3	mViewDirection;
    glm::quat	mOrientation;
    glm::vec3	mWorldUp;

    float	mFov; // vertical field of view in degrees
    float	mAspectRatio;
    float	mNearClip;
    float	mFarClip;
    float	mPivotDistance;

    mutable glm::vec3	mU;	// Right vector
    mutable glm::vec3	mV;	// Readjust up-vector
    mutable glm::vec3	mW;	// Negative view direction

    mutable glm::mat4	mProjectionMatrix, mInverseProjectionMatrix;
    mutable bool	mProjectionCached;
    mutable glm::mat4	mViewMatrix;
    mutable bool	mModelViewCached;
    mutable glm::mat4	mInverseModelViewMatrix;
    mutable bool	mInverseModelViewCached;

    mutable float	mFrustumLeft, mFrustumRight, mFrustumTop, mFrustumBottom;
};

//! A perspective Camera.
class CameraPersp : public Camera {
  public:
    //! Creates a default camera with eyePoint at ( 28, 21, 28 ), looking at the origin, 35deg vertical field-of-view and a 1.333 aspect ratio.
    CameraPersp();
    //! Constructs screen-aligned camera
    CameraPersp( int pixelWidth, int pixelHeight, float fov );
    //! Constructs screen-aligned camera
    CameraPersp( int pixelWidth, int pixelHeight, float fov, float nearPlane, float farPlane );

    //! Configures the camera's projection according to the provided parameters.
    void	setPerspective( float verticalFovDegrees, float aspectRatio, float nearPlane, float farPlane );

    /** Returns both the horizontal and vertical lens shift.
        A horizontal lens shift of 1 (-1) will shift the view right (left) by half the width of the viewport.
        A vertical lens shift of 1 (-1) will shift the view up (down) by half the height of the viewport. */
    void	getLensShift( float *horizontal, float *vertical ) const { *horizontal = mLensShift.x; *vertical = mLensShift.y; }
    /** Returns both the horizontal and vertical lens shift.
        A horizontal lens shift of 1 (-1) will shift the view right (left) by half the width of the viewport.
        A vertical lens shift of 1 (-1) will shift the view up (down) by half the height of the viewport. */
    glm::vec2	getLensShift() const { return mLensShift; }
    /** Sets both the horizontal and vertical lens shift.
        A horizontal lens shift of 1 (-1) will shift the view right (left) by half the width of the viewport.
        A vertical lens shift of 1 (-1) will shift the view up (down) by half the height of the viewport. */
    void	setLensShift( float horizontal, float vertical );
    /** Sets both the horizontal and vertical lens shift.
        A horizontal lens shift of 1 (-1) will shift the view right (left) by half the width of the viewport.
        A vertical lens shift of 1 (-1) will shift the view up (down) by half the height of the viewport. */
    void	setLensShift( const glm::vec2 &shift ) { setLensShift( shift.x, shift.y ); }
    //! Returns the horizontal lens shift. A horizontal lens shift of 1 (-1) will shift the view right (left) by half the width of the viewport.
    float	getLensShiftHorizontal() const { return mLensShift.x; }
    /** Sets the horizontal lens shift.
        A horizontal lens shift of 1 (-1) will shift the view right (left) by half the width of the viewport. */
    void	setLensShiftHorizontal( float horizontal ) { setLensShift( horizontal, mLensShift.y ); }
    //! Returns the vertical lens shift. A vertical lens shift of 1 (-1) will shift the view up (down) by half the height of the viewport.
    float	getLensShiftVertical() const { return mLensShift.y; }
    /** Sets the vertical lens shift.
        A vertical lens shift of 1 (-1) will shift the view up (down) by half the height of the viewport. */
    void	setLensShiftVertical( float vertical ) { setLensShift( mLensShift.x, vertical ); }

    bool	isPersp() const override { return true; }

    //! Returns a Camera whose eyePoint is positioned to exactly frame \a worldSpaceSphere but is equivalent in other parameters (including orientation). Sets the result's pivotDistance to be the distance to \a worldSpaceSphere's center.
//	CameraPersp		calcFraming( const Sphere &worldSpaceSphere ) const;

  protected:
    glm::vec2	mLensShift;

    void	calcProjection() const override;
//	Ray		calcRay( float u, float v, float imagePlaneAspectRatio ) const override;
};

//! An orthographic Camera.
class CameraOrtho : public Camera {
  public:
    CameraOrtho();
    CameraOrtho( float left, float right, float bottom, float top, float nearPlane, float farPlane );

    void	setOrtho( float left, float right, float bottom, float top, float nearPlane, float farPlane );

    bool	isPersp() const override { return false; }

  protected:
    void	calcProjection() const override;
};

//! A Camera used for stereoscopic displays.
class CameraStereo : public CameraPersp {
  public:
    CameraStereo()
        : mIsStereo( false ), mIsLeft( true ), mConvergence( 1.0f ), mEyeSeparation( 0.05f ) {}
    CameraStereo( int pixelWidth, int pixelHeight, float fov )
        : CameraPersp( pixelWidth, pixelHeight, fov ),
          mIsStereo( false ), mIsLeft( true ), mConvergence( 1.0f ), mEyeSeparation( 0.05f ) {} // constructs screen-aligned camera
    CameraStereo( int pixelWidth, int pixelHeight, float fov, float nearPlane, float farPlane )
        : CameraPersp( pixelWidth, pixelHeight, fov, nearPlane, farPlane ),
          mIsStereo( false ), mIsLeft( true ), mConvergence( 1.0f ), mEyeSeparation( 0.05f ) {} // constructs screen-aligned camera

    //! Returns the current convergence, which is the distance at which there is no parallax.
    float			getConvergence() const { return mConvergence; }
    //! Sets the convergence of the camera, which is the distance at which there is no parallax.
    void			setConvergence( float distance, bool adjustEyeSeparation = false );

    //! Returns the distance between the camera's for the left and right eyes.
    float			getEyeSeparation() const { return mEyeSeparation; }
    //! Sets the distance between the camera's for the left and right eyes. This affects the parallax effect.
    void			setEyeSeparation( float distance ) { mEyeSeparation = distance; mModelViewCached = false; mProjectionCached = false; }
    //! Returns the location of the currently enabled eye camera.
    glm::vec3			getEyePointShifted() const;

    //! Enables the left eye camera.
    void			enableStereoLeft() { mIsStereo = true; mIsLeft = true; }
    //! Returns whether the left eye camera is enabled.
    bool			isStereoLeftEnabled() const { return mIsStereo && mIsLeft; }
    //! Enables the right eye camera.
    void			enableStereoRight() { mIsStereo = true; mIsLeft = false; }
    //! Returns whether the right eye camera is enabled.
    bool			isStereoRightEnabled() const { return mIsStereo && ! mIsLeft; }
    //! Disables stereoscopic rendering, converting the camera to a standard CameraPersp.
    void			disableStereo() { mIsStereo = false; }
    //! Returns whether stereoscopic rendering is enabled.
    bool			isStereoEnabled() const { return mIsStereo; }

    void	getNearClipCoordinates( glm::vec3 *topLeft, glm::vec3 *topRight, glm::vec3 *bottomLeft, glm::vec3 *bottomRight ) const override;
    void	getFarClipCoordinates( glm::vec3 *topLeft, glm::vec3 *topRight, glm::vec3 *bottomLeft, glm::vec3 *bottomRight ) const override;

    const glm::mat4&	getProjectionMatrix() const override;
    const glm::mat4&	getViewMatrix() const override;
    const glm::mat4&	getInverseViewMatrix() const override;

  protected:
    mutable glm::mat4	mProjectionMatrixLeft, mInverseProjectionMatrixLeft;
    mutable glm::mat4	mProjectionMatrixRight, mInverseProjectionMatrixRight;
    mutable glm::mat4	mViewMatrixLeft, mInverseModelViewMatrixLeft;
    mutable glm::mat4	mViewMatrixRight, mInverseModelViewMatrixRight;

    void	calcViewMatrix() const override;
    void	calcInverseView() const override;
    void	calcProjection() const override;

  private:
    bool			mIsStereo;
    bool			mIsLeft;

    float			mConvergence;
    float			mEyeSeparation;
};

// Just a quick hacked-up "physical" representation of a Camera -- it has momentum,
// it can be pushed around, and can be constrained to move within an AABB.
class CameraDolly
{
public:
  explicit CameraDolly(Camera &cam) :
      camera_(cam),
      velocity_(0,0,0),
      drag_coeff_(0.5f),
      pos_min_(-FLT_MAX, -FLT_MAX, -FLT_MAX),
      pos_max_(FLT_MAX, FLT_MAX, FLT_MAX) {
  }
  ~CameraDolly() = default;
  CameraDolly& operator=(const CameraDolly&) = delete;

  // If SetBounds() isn't called, the default bounds are +/-FLT_MAX.
  void SetBounds(glm::vec3 aabb_min, glm::vec3 aabb_max) {
    pos_min_ = aabb_min;
    pos_max_ = aabb_max;
  }
  Camera& GetCamera() { return camera_; }
  const Camera& GetCamera() const { return camera_; }
  void Update(glm::vec3 accel, float dt);

private:
  Camera& camera_;
  glm::vec3 velocity_;
  float drag_coeff_;
  glm::vec3 pos_min_, pos_max_;
};

#endif //!defined(CAMERA_H)
