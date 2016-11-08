#if !defined(CAMERA_H)
#define CAMERA_H

#include <mathfu/glsl_mappings.h>

class Camera {
  public:
	virtual ~Camera() {}

	//! Returns the position in world-space from which the Camera is viewing
	mathfu::vec3		getEyePoint() const { return mEyePoint; }
	//! Sets the position in world-space from which the Camera is viewing
	void		setEyePoint( const mathfu::vec3 &eyePoint );

	//! Returns the vector in world-space which represents "up" - typically mathfu::vec3( 0, 1, 0 ) 
	mathfu::vec3		getWorldUp() const { return mWorldUp; }
	//! Sets the vector in world-space which represents "up" - typically mathfu::vec3( 0, 1, 0 )
	void		setWorldUp( const mathfu::vec3 &worldUp );

	//! Modifies the view direction to look from the current eyePoint to \a target. Also updates the pivot distance.
	void		lookAt( const mathfu::vec3 &target );
	//! Modifies the eyePoint and view direction to look from \a eyePoint to \a target. Also updates the pivot distance.
	void		lookAt( const mathfu::vec3 &eyePoint, const mathfu::vec3 &target );
	//! Modifies the eyePoint and view direction to look from \a eyePoint to \a target with up vector \a up (to achieve camera roll). Also updates the pivot distance.
	void		lookAt( const mathfu::vec3 &eyePoint, const mathfu::vec3 &target, const mathfu::vec3 &up );
	//! Returns the world-space vector along which the camera is oriented
	mathfu::vec3		getViewDirection() const { return mViewDirection; }
	//! Sets the world-space vector along which the camera is oriented
	void		setViewDirection( const mathfu::vec3 &viewDirection );

	//! Returns the world-space quaternion that expresses the camera's orientation
	mathfu::quat		getOrientation() const { return mOrientation; }
	//! Sets the camera's orientation with world-space quaternion \a orientation
	void		setOrientation( const mathfu::quat &orientation );

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
	mathfu::vec3	getPivotPoint() const { return mEyePoint + mViewDirection * mPivotDistance; }

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
	virtual void	getNearClipCoordinates( mathfu::vec3 *topLeft, mathfu::vec3 *topRight, mathfu::vec3 *bottomLeft, mathfu::vec3 *bottomRight ) const;
	//! Returns the four corners of the Camera's Far clipping plane, expressed in world-space
	virtual void	getFarClipCoordinates( mathfu::vec3 *topLeft, mathfu::vec3 *topRight, mathfu::vec3 *bottomLeft, mathfu::vec3 *bottomRight ) const;

	//! Returns the coordinates of the camera's frustum, suitable for passing to \c glFrustum
	void	getFrustum( float *left, float *top, float *right, float *bottom, float *near, float *far ) const;
	//! Returns whether the camera represents a perspective projection instead of an orthographic
	virtual bool isPersp() const = 0;
	
	//! Returns the Camera's Projection matrix, which converts view-space into clip-space
	virtual const mathfu::mat4&	getProjectionMatrix() const { if( ! mProjectionCached ) calcProjection(); return mProjectionMatrix; }
	//! Returns the Camera's View matrix, which converts world-space into view-space
	virtual const mathfu::mat4&	getViewMatrix() const { if( ! mModelViewCached ) calcViewMatrix(); return mViewMatrix; }
	//! Returns the Camera's Inverse View matrix, which converts view-space into world-space
	virtual const mathfu::mat4&	getInverseViewMatrix() const { if( ! mInverseModelViewCached ) calcInverseView(); return mInverseModelViewMatrix; }

	//! Returns a Ray that passes through the image plane coordinates (\a u, \a v) (expressed in the range [0,1]) on an image plane of aspect ratio \a imagePlaneAspectRatio
//	Ray		generateRay( float u, float v, float imagePlaneAspectRatio ) const { return calcRay( u, v, imagePlaneAspectRatio ); }
	//! Returns a Ray that passes through the pixels coordinates \a posPixels on an image of size \a imageSizePixels
//	Ray		generateRay( const mathfu::vec2 &posPixels, const mathfu::vec2 &imageSizePixels ) const { return calcRay( posPixels.x / imageSizePixels.x, ( imageSizePixels.y - posPixels.y ) / imageSizePixels.y, imageSizePixels.x / imageSizePixels.y ); }
	//! Returns the \a right and \a up vectors suitable for billboarding relative to the Camera
	void	getBillboardVectors( mathfu::vec3 *right, mathfu::vec3 *up ) const;

	//! Converts a world-space coordinate \a worldCoord to screen coordinates as viewed by the camera, based on a screen which is \a screenWidth x \a screenHeight pixels.
	mathfu::vec2 worldToScreen( const mathfu::vec3 &worldCoord, float screenWidth, float screenHeight ) const;
	//! Converts a eye-space coordinate \a eyeCoord to screen coordinates as viewed by the camera
	mathfu::vec2 eyeToScreen( const mathfu::vec3 &eyeCoord, const mathfu::vec2 &screenSizePixels ) const;
	//! Converts a world-space coordinate \a worldCoord to eye-space, also known as camera-space. -Z is along the view direction.
	mathfu::vec3 worldToEye( const mathfu::vec3 &worldCoord ) const	{ return (getViewMatrix() * mathfu::vec4( worldCoord, 1 )).xyz(); }
	//! Converts a world-space coordinate \a worldCoord to the z axis of eye-space, also known as camera-space. -Z is along the view direction. Suitable for depth sorting.
	float worldToEyeDepth( const mathfu::vec3 &worldCoord ) const;
	//! Converts a world-space coordinate \a worldCoord to normalized device coordinates
	mathfu::vec3 worldToNdc( const mathfu::vec3 &worldCoord ) const;

	//! Calculates the area of the screen-space elliptical projection of \a sphere
//	float	calcScreenArea( const Sphere &sphere, const mathfu::vec2 &screenSizePixels ) const;
	//! Calculates the screen-space elliptical projection of \a sphere, putting the results in \a outCenter, \a outAxisA and \a outAxisB
//	void	calcScreenProjection( const Sphere &sphere, const mathfu::vec2 &screenSizePixels, mathfu::vec2 *outCenter, mathfu::vec2 *outAxisA, mathfu::vec2 *outAxisB ) const;

  protected:
	Camera()
		: mModelViewCached( false ), mProjectionCached( false ), mInverseModelViewCached( false ), mWorldUp( mathfu::vec3( 0, 1, 0 ) ),
			mPivotDistance( 0 )
	{}

	void			calcMatrices() const;

	virtual void	calcViewMatrix() const;
	virtual void	calcInverseView() const;
	virtual void	calcProjection() const = 0;

//	virtual Ray		calcRay( float u, float v, float imagePlaneAspectRatio ) const;

	mathfu::vec3	mEyePoint;
	mathfu::vec3	mViewDirection;
	mathfu::quat	mOrientation;
	mathfu::vec3	mWorldUp;

	float	mFov; // vertical field of view in degrees
	float	mAspectRatio;
	float	mNearClip;		
	float	mFarClip;
	float	mPivotDistance;

	mutable mathfu::vec3	mU;	// Right vector
	mutable mathfu::vec3	mV;	// Readjust up-vector
	mutable mathfu::vec3	mW;	// Negative view direction

	mutable mathfu::mat4	mProjectionMatrix, mInverseProjectionMatrix;
	mutable bool	mProjectionCached;
	mutable mathfu::mat4	mViewMatrix;
	mutable bool	mModelViewCached;
	mutable mathfu::mat4	mInverseModelViewMatrix;
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
	void	getLensShift( float *horizontal, float *vertical ) const { *horizontal = mLensShift.x(); *vertical = mLensShift.y(); }
	/** Returns both the horizontal and vertical lens shift. 
		A horizontal lens shift of 1 (-1) will shift the view right (left) by half the width of the viewport.
		A vertical lens shift of 1 (-1) will shift the view up (down) by half the height of the viewport. */
	mathfu::vec2	getLensShift() const { return mLensShift; }
	/** Sets both the horizontal and vertical lens shift. 
		A horizontal lens shift of 1 (-1) will shift the view right (left) by half the width of the viewport.
		A vertical lens shift of 1 (-1) will shift the view up (down) by half the height of the viewport. */
	void	setLensShift( float horizontal, float vertical );
	/** Sets both the horizontal and vertical lens shift. 
		A horizontal lens shift of 1 (-1) will shift the view right (left) by half the width of the viewport.
		A vertical lens shift of 1 (-1) will shift the view up (down) by half the height of the viewport. */
	void	setLensShift( const mathfu::vec2 &shift ) { setLensShift( shift.x(), shift.y() ); }
	//! Returns the horizontal lens shift. A horizontal lens shift of 1 (-1) will shift the view right (left) by half the width of the viewport.
	float	getLensShiftHorizontal() const { return mLensShift.x(); }
	/** Sets the horizontal lens shift. 
		A horizontal lens shift of 1 (-1) will shift the view right (left) by half the width of the viewport. */
	void	setLensShiftHorizontal( float horizontal ) { setLensShift( horizontal, mLensShift.y() ); }
	//! Returns the vertical lens shift. A vertical lens shift of 1 (-1) will shift the view up (down) by half the height of the viewport.
	float	getLensShiftVertical() const { return mLensShift.y(); }
	/** Sets the vertical lens shift. 
		A vertical lens shift of 1 (-1) will shift the view up (down) by half the height of the viewport. */
	void	setLensShiftVertical( float vertical ) { setLensShift( mLensShift.x(), vertical ); }
	
	bool	isPersp() const override { return true; }

	//! Returns a Camera whose eyePoint is positioned to exactly frame \a worldSpaceSphere but is equivalent in other parameters (including orientation). Sets the result's pivotDistance to be the distance to \a worldSpaceSphere's center.
//	CameraPersp		calcFraming( const Sphere &worldSpaceSphere ) const;

  protected:
	mathfu::vec2	mLensShift;

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
		: mConvergence( 1.0f ), mEyeSeparation( 0.05f ), mIsStereo( false ), mIsLeft( true ) {}
	CameraStereo( int pixelWidth, int pixelHeight, float fov )
		: CameraPersp( pixelWidth, pixelHeight, fov ), 
		mConvergence( 1.0f ), mEyeSeparation( 0.05f ), mIsStereo( false ), mIsLeft( true ) {} // constructs screen-aligned camera
	CameraStereo( int pixelWidth, int pixelHeight, float fov, float nearPlane, float farPlane )
		: CameraPersp( pixelWidth, pixelHeight, fov, nearPlane, farPlane ), 
		mConvergence( 1.0f ), mEyeSeparation( 0.05f ), mIsStereo( false ), mIsLeft( true ) {} // constructs screen-aligned camera

	//! Returns the current convergence, which is the distance at which there is no parallax.
	float			getConvergence() const { return mConvergence; }
	//! Sets the convergence of the camera, which is the distance at which there is no parallax.
	void			setConvergence( float distance, bool adjustEyeSeparation = false );
	
	//! Returns the distance between the camera's for the left and right eyes.
	float			getEyeSeparation() const { return mEyeSeparation; }
	//! Sets the distance between the camera's for the left and right eyes. This affects the parallax effect. 
	void			setEyeSeparation( float distance ) { mEyeSeparation = distance; mModelViewCached = false; mProjectionCached = false; }
	//! Returns the location of the currently enabled eye camera.
	mathfu::vec3			getEyePointShifted() const;
	
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

	void	getNearClipCoordinates( mathfu::vec3 *topLeft, mathfu::vec3 *topRight, mathfu::vec3 *bottomLeft, mathfu::vec3 *bottomRight ) const override;
	void	getFarClipCoordinates( mathfu::vec3 *topLeft, mathfu::vec3 *topRight, mathfu::vec3 *bottomLeft, mathfu::vec3 *bottomRight ) const override;
	
	const mathfu::mat4&	getProjectionMatrix() const override;
	const mathfu::mat4&	getViewMatrix() const override;
	const mathfu::mat4&	getInverseViewMatrix() const override;

  protected:
	mutable mathfu::mat4	mProjectionMatrixLeft, mInverseProjectionMatrixLeft;
	mutable mathfu::mat4	mProjectionMatrixRight, mInverseProjectionMatrixRight;
	mutable mathfu::mat4	mViewMatrixLeft, mInverseModelViewMatrixLeft;
	mutable mathfu::mat4	mViewMatrixRight, mInverseModelViewMatrixRight;

	void	calcViewMatrix() const override;
	void	calcInverseView() const override;
	void	calcProjection() const override;
	
  private:
	bool			mIsStereo;
	bool			mIsLeft;

	float			mConvergence;
	float			mEyeSeparation;
};

#endif //!defined(CAMERA_H)
