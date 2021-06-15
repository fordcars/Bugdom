/****************************/
/*   	QD3D SUPPORT.C	    */
/* (c)1997-99 Pangea Software  */
/* By Brian Greenstone      */
/****************************/


/****************************/
/*    EXTERNALS             */
/****************************/

#include <SDL_opengl.h>
#include "game.h"


/****************************/
/*    PROTOTYPES            */
/****************************/

static void CreateDrawContext(QD3DViewDefType *viewDefPtr);
static void SetStyles(QD3DStyleDefType *styleDefPtr);
static void CreateCamera(QD3DSetupInputType *setupDefPtr);
static void CreateLights(QD3DLightDefType *lightDefPtr);
static void CreateView(QD3DSetupInputType *setupDefPtr);
static void DrawPICTIntoMipmap(PicHandle pict,long width, long height, TQ3Mipmap *mipmap, Boolean blackIsAlpha);
static void Data16ToMipmap(Ptr data, short width, short height, TQ3Mipmap *mipmap);
static void DrawNormal(TQ3ViewObject view);


/****************************/
/*    CONSTANTS             */
/****************************/



/*********************/
/*    VARIABLES      */
/*********************/


SDL_GLContext					gGLContext;
RenderStats						gRenderStats;

int								gWindowWidth				= GAME_VIEW_WIDTH;
int								gWindowHeight				= GAME_VIEW_HEIGHT;

float	gFramesPerSecond = DEFAULT_FPS;				// this is used to maintain a constant timing velocity as frame rates differ
float	gFramesPerSecondFrac = 1/DEFAULT_FPS;

Boolean		gQD3DInitialized = false;


		/* SOURCE PORT EXTRAS */

// Source port addition: this is a Quesa feature, enabled by default,
// that renders translucent materials more accurately at an angle.
// However, it looks "off" in the game -- shadow quads, shield spheres,
// water patches all appear darker than they would on original hardware.
static const TQ3Boolean gQD3D_AngleAffectsAlpha = kQ3False;

// Source port addition: don't let Quesa swap buffers automatically
// because we render stuff outside Quesa (such as the HUD).
static const TQ3Boolean gQD3D_SwapBufferInEndPass = kQ3False;

static Boolean gQD3D_FreshDrawContext = false;



		/* DEBUG STUFF */
		
static TQ3Point3D		gNormalWhere;
static TQ3Vector3D		gNormal;




/******************** QD3D: BOOT ******************************/
//
// NOTE: The QuickDraw3D libraries should be included in the project as a "WEAK LINK" so that I can
// 		get an error if the library can't load.  Otherwise, the Finder reports a useless error to the user.
//

void QD3D_Boot(void)
{
	printf("TODO NOQUESA: %s\n", __func__);
#if 0	// NOQUESA
TQ3Status	status;


				/* LET 'ER RIP! */
				
	status = Q3Initialize();
	GAME_ASSERT(status);

	gQD3DInitialized = true;
#endif
}



//=======================================================================================================
//=============================== VIEW WINDOW SETUP STUFF ===============================================
//=======================================================================================================


/*********************** QD3D: NEW VIEW DEF **********************/
//
// fills a view def structure with default values.
//

void QD3D_NewViewDef(QD3DSetupInputType *viewDef, WindowPtr theWindow)
{
TQ3ColorRGBA		clearColor = {0,0,0,1};
TQ3Point3D			cameraFrom = { 0, 40, 200.0 };
TQ3Point3D			cameraTo = { 0, 0, 0 };
TQ3Vector3D			cameraUp = { 0.0, 1.0, 0.0 };
TQ3ColorRGB			ambientColor = { 1.0, 1.0, .8 };
TQ3Vector3D			fillDirection1 = { 1, -1, .3 };
TQ3Vector3D			fillDirection2 = { -.8, .8, -.2 };

	Q3Vector3D_Normalize(&fillDirection1,&fillDirection1);
	Q3Vector3D_Normalize(&fillDirection2,&fillDirection2);

	if (theWindow == nil)
		viewDef->view.useWindow 	=	false;							// assume going to pixmap
	else
		viewDef->view.useWindow 	=	true;							// assume going to window
	viewDef->view.displayWindow 	= theWindow;
//	viewDef->view.rendererType 		= kQ3RendererTypeOpenGL;
	viewDef->view.clearColor 		= clearColor;
	viewDef->view.paneClip.left 	= 0;
	viewDef->view.paneClip.right 	= 0;
	viewDef->view.paneClip.top 		= 0;
	viewDef->view.paneClip.bottom 	= 0;
	viewDef->view.dontClear			= false;

	viewDef->styles.interpolation 	= kQ3InterpolationStyleVertex; 
	viewDef->styles.backfacing 		= kQ3BackfacingStyleRemove; 
	viewDef->styles.fill			= kQ3FillStyleFilled; 
	viewDef->styles.usePhong 		= false; 

	viewDef->camera.from 			= cameraFrom;
	viewDef->camera.to 				= cameraTo;
	viewDef->camera.up 				= cameraUp;
	viewDef->camera.hither 			= 10;
	viewDef->camera.yon 			= 3000;
	viewDef->camera.fov 			= .9;

	viewDef->lights.ambientBrightness = 0.1;
	viewDef->lights.ambientColor 	= ambientColor;
	viewDef->lights.numFillLights 	= 2;
	viewDef->lights.fillDirection[0] = fillDirection1;
	viewDef->lights.fillDirection[1] = fillDirection2;
	viewDef->lights.fillColor[0] 	= ambientColor;
	viewDef->lights.fillColor[1] 	= ambientColor;
	viewDef->lights.fillBrightness[0] = .9;
	viewDef->lights.fillBrightness[1] = .2;
	
	viewDef->lights.useFog 		= true;
	viewDef->lights.fogStart 	= .8;
	viewDef->lights.fogEnd 		= 1.0;
	viewDef->lights.fogDensity	= 1.0;
	viewDef->lights.fogMode		= kQ3FogModePlaneBasedLinear;
	viewDef->lights.useCustomFogColor = false;
	viewDef->lights.fogColor	= clearColor;
	
	viewDef->enableMultisamplingByDefault = true;
}

/************** SETUP QD3D WINDOW *******************/

void QD3D_SetupWindow(QD3DSetupInputType *setupDefPtr, QD3DSetupOutputType **outputHandle)
{
TQ3Vector3D	v = {0,0,0};
QD3DSetupOutputType	*outputPtr;

			/* ALLOC MEMORY FOR OUTPUT DATA */

	*outputHandle = (QD3DSetupOutputType *)AllocPtr(sizeof(QD3DSetupOutputType));
	outputPtr = *outputHandle;
	GAME_ASSERT(outputPtr);

				/* CREATE & SET DRAW CONTEXT */

	gGLContext = SDL_GL_CreateContext(gSDLWindow);									// also makes it current
	GAME_ASSERT(gGLContext);

				/* SETUP */

	CreateView(setupDefPtr);
	
	CreateCamera(setupDefPtr);										// create new CAMERA object
	CreateLights(&setupDefPtr->lights);
	SetStyles(&setupDefPtr->styles);	

				/* PASS BACK INFO */

#if 0	// NOQUESA
	outputPtr->viewObject 			= gQD3D_ViewObject;
	outputPtr->interpolationStyle 	= gQD3D_InterpolationStyle;
	outputPtr->fillStyle 			= gQD3D_FillStyle;
	outputPtr->backfacingStyle 		= gQD3D_BackfacingStyle;
	outputPtr->shaderObject 		= gQD3D_ShaderObject;
	outputPtr->nullShaderObject 	= gQD3D_NullShaderObject;
	outputPtr->cameraObject 		= gQD3D_CameraObject;
	outputPtr->lightGroup 			= gQD3D_LightGroup;
	outputPtr->drawContext 			= gQD3D_DrawContext;
#endif
	outputPtr->window 				= setupDefPtr->view.displayWindow;	// remember which window
	outputPtr->paneClip 			= setupDefPtr->view.paneClip;
	outputPtr->aspectRatio			= 1.0f;								// aspect ratio is set at every frame depending on window size
	outputPtr->needScissorTest 		= setupDefPtr->view.paneClip.left != 0 || setupDefPtr->view.paneClip.right != 0
									|| setupDefPtr->view.paneClip.bottom != 0 || setupDefPtr->view.paneClip.top != 0;
	outputPtr->hither 				= setupDefPtr->camera.hither;		// remember hither/yon
	outputPtr->yon 					= setupDefPtr->camera.yon;
	outputPtr->fov					= setupDefPtr->camera.fov;
	outputPtr->enableMultisamplingByDefault = setupDefPtr->enableMultisamplingByDefault;

	outputPtr->currentCameraUpVector	= setupDefPtr->camera.up;
	outputPtr->currentCameraLookAt		= setupDefPtr->camera.to;
	outputPtr->currentCameraCoords		= setupDefPtr->camera.from;

	outputPtr->isActive = true;								// it's now an active structure
	
	outputPtr->lightList = setupDefPtr->lights;				// copy light list
	
	QD3D_MoveCameraFromTo(outputPtr,&v,&v);					// call this to set outputPtr->currentCameraCoords & camera matrix



				/* SET UP OPENGL RENDERER PROPERTIES NOW THAT WE HAVE A CONTEXT */

	SDL_GL_SetSwapInterval(gGamePrefs.vsync ? 1 : 0);

	CreateLights(&setupDefPtr->lights);

	glAlphaFunc(GL_GREATER, 0.4999f);

	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	// Normalize normal vectors. Required so lighting looks correct on scaled meshes.
	glEnable(GL_NORMALIZE);

	glCullFace(GL_BACK);
	glFrontFace(GL_CCW);									// CCW is front face

	glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);

	Render_InitState();
//	Render_Alloc2DCover(GAME_VIEW_WIDTH, GAME_VIEW_HEIGHT);

	if (setupDefPtr->lights.useFog)
	{
		Render_EnableFog(
				setupDefPtr->camera.hither,
				setupDefPtr->camera.yon,
				setupDefPtr->lights.fogStart,
				setupDefPtr->lights.fogEnd,
				setupDefPtr->lights.useCustomFogColor ? setupDefPtr->lights.fogColor : setupDefPtr->view.clearColor);
	}
	else
		Render_DisableFog();

	glClearColor(setupDefPtr->view.clearColor.r, setupDefPtr->view.clearColor.g, setupDefPtr->view.clearColor.b, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	CHECK_GL_ERROR();

}


/***************** QD3D_DisposeWindowSetup ***********************/
//
// Disposes of all data created by QD3D_SetupWindow
//

void QD3D_DisposeWindowSetup(QD3DSetupOutputType **dataHandle)
{
QD3DSetupOutputType	*data;

	data = *dataHandle;
	GAME_ASSERT(data);										// see if this setup exists

//	Render_Dispose2DCover(); // Source port addition - release backdrop GL texture

	SDL_GL_DeleteContext(gGLContext);						// dispose GL context
	gGLContext = nil;

	data->isActive = false;									// now inactive
	
		/* FREE MEMORY & NIL POINTER */
		
	DisposePtr((Ptr)data);
	*dataHandle = nil;
}


/******************* CREATE GAME VIEW *************************/

static void CreateView(QD3DSetupInputType *setupDefPtr)
{
	printf("TODO NOQUESA: %s\n", __func__);
#if 0	// NOQUESA
TQ3Status	status;

				/* CREATE NEW VIEW OBJECT */
				
	gQD3D_ViewObject = Q3View_New();
	GAME_ASSERT(gQD3D_ViewObject);

			/* CREATE & SET DRAW CONTEXT */
	
	CreateDrawContext(&setupDefPtr->view); 											// init draw context
	
	status = Q3View_SetDrawContext(gQD3D_ViewObject, gQD3D_DrawContext);			// assign context to view
	GAME_ASSERT(status);

			/* CREATE & SET RENDERER */

	gQD3D_RendererObject = Q3Renderer_NewFromType(setupDefPtr->view.rendererType);	// create new RENDERER object
	GAME_ASSERT(gQD3D_RendererObject);

	status = Q3View_SetRenderer(gQD3D_ViewObject, gQD3D_RendererObject);				// assign renderer to view
	GAME_ASSERT(status);
				
		
		/* SET RENDERER FEATURES */
		
#if 0
	TQ3Uns32	hints;
	Q3InteractiveRenderer_GetRAVEContextHints(gQD3D_RendererObject, &hints);
	hints &= ~kQAContext_NoZBuffer; 				// Z buffer is on 
	hints &= ~kQAContext_DeepZ; 					// shallow z
	hints &= ~kQAContext_NoDither; 					// yes-dither
	Q3InteractiveRenderer_SetRAVEContextHints(gQD3D_RendererObject, hints);	
#endif

	// Source port addition: set bilinear texturing according to user preference
	Q3InteractiveRenderer_SetRAVETextureFilter(
		gQD3D_RendererObject,
		gGamePrefs.textureFiltering ? kQATextureFilter_Best : kQATextureFilter_Fast);

#if 0
	Q3InteractiveRenderer_SetDoubleBufferBypass(gQD3D_RendererObject,kQ3True);
#endif

	// Source port addition: turn off Quesa's angle affect on alpha to preserve the original look of shadows, water, shields etc.
	Q3Object_SetProperty(gQD3D_RendererObject, kQ3RendererPropertyAngleAffectsAlpha,
						 sizeof(gQD3D_AngleAffectsAlpha), &gQD3D_AngleAffectsAlpha);

	// Source port addition: we draw overlays in straight OpenGL, so we want control over when the buffers are swapped.
	Q3Object_SetProperty(gQD3D_DrawContext, kQ3DrawContextPropertySwapBufferInEndPass,
						sizeof(gQD3D_SwapBufferInEndPass), &gQD3D_SwapBufferInEndPass);

	// Source port addition: Enable writing transparent stuff into the z-buffer.
	// (This makes auto-fading stuff look better)
	static const TQ3Float32 depthAlphaThreshold = 0.99f;
	Q3Object_SetProperty(gQD3D_RendererObject, kQ3RendererPropertyDepthAlphaThreshold,
					  sizeof(depthAlphaThreshold), &depthAlphaThreshold);

	// Uncomment to apply an alpha threshold to EVERYTHING in the game
//	static const TQ3Float32 gQD3D_AlphaThreshold = 0.501337;
//	Q3Object_SetProperty(gQD3D_RendererObject, kQ3RendererPropertyAlphaThreshold, sizeof(gQD3D_AlphaThreshold), &gQD3D_AlphaThreshold);
#endif
}


/**************** CREATE DRAW CONTEXT *********************/

static void CreateDrawContext(QD3DViewDefType *viewDefPtr)
{
	printf("TODO NOQUESA: %s\n", __func__);
#if 0	// NOQUESA
TQ3DrawContextData		drawContexData;
TQ3SDLDrawContextData	myMacDrawContextData;
extern SDL_Window*		gSDLWindow;

	int ww, wh;
	SDL_GL_GetDrawableSize(gSDLWindow, &ww, &wh);

			/* SEE IF DOING PIXMAP CONTEXT */
			
	GAME_ASSERT_MESSAGE(viewDefPtr->useWindow, "Pixmap context not supported!");

			/* FILL IN DRAW CONTEXT DATA */

	if (viewDefPtr->dontClear)
		drawContexData.clearImageMethod = kQ3ClearMethodNone;
	else
		drawContexData.clearImageMethod = kQ3ClearMethodWithColor;		
	
	drawContexData.clearImageColor = viewDefPtr->clearColor;				// color to clear to
	drawContexData.pane = GetAdjustedPane(ww, wh, GAME_VIEW_WIDTH, GAME_VIEW_HEIGHT, viewDefPtr->paneClip);

#if DEBUG_WIREFRAME
	drawContexData.clearImageColor.a = 1.0f;
	drawContexData.clearImageColor.r = 0.0f;
	drawContexData.clearImageColor.g = 0.5f;
	drawContexData.clearImageColor.b = 1.0f;
#endif

	drawContexData.paneState = kQ3True;										// use bounds?
	drawContexData.maskState = kQ3False;									// no mask
	drawContexData.doubleBufferState = kQ3True;								// double buffering

	myMacDrawContextData.drawContextData = drawContexData;					// set MAC specifics
	myMacDrawContextData.sdlWindow = gSDLWindow;							// assign window to draw to


			/* CREATE DRAW CONTEXT */

	gQD3D_DrawContext = Q3SDLDrawContext_New(&myMacDrawContextData);
	GAME_ASSERT(gQD3D_DrawContext);


	gQD3D_FreshDrawContext = true;
#endif
}


/**************** SET STYLES ****************/
//
// Creates style objects which define how the scene is to be rendered.
// It also sets the shader object.
//

static void SetStyles(QD3DStyleDefType *styleDefPtr)
{
	printf("TODO NOQUESA: %s\n", __func__);
#if 0	// NOQUESA
				/* SET INTERPOLATION (FOR SHADING) */
					
	gQD3D_InterpolationStyle = Q3InterpolationStyle_New(styleDefPtr->interpolation);
	GAME_ASSERT(gQD3D_InterpolationStyle);

					/* SET BACKFACING */

	gQD3D_BackfacingStyle = Q3BackfacingStyle_New(styleDefPtr->backfacing);
	GAME_ASSERT(gQD3D_BackfacingStyle);

				/* SET POLYGON FILL STYLE */
						
#if DEBUG_WIREFRAME
	gQD3D_FillStyle = Q3FillStyle_New(kQ3FillStyleEdges);
#else
	gQD3D_FillStyle = Q3FillStyle_New(styleDefPtr->fill);
#endif
	GAME_ASSERT(gQD3D_FillStyle);


					/* SET THE SHADER TO USE */

	if (styleDefPtr->usePhong)
	{
		gQD3D_ShaderObject = Q3PhongIllumination_New();
		GAME_ASSERT(gQD3D_ShaderObject);
	}
	else
	{
		gQD3D_ShaderObject = Q3LambertIllumination_New();
		GAME_ASSERT(gQD3D_ShaderObject);
	}


			/* ALSO MAKE NULL SHADER FOR SPECIAL PURPOSES */
			
	gQD3D_NullShaderObject = Q3NULLIllumination_New();
#endif
}



/****************** CREATE CAMERA *********************/

static void CreateCamera(QD3DSetupInputType *setupDefPtr)
{
	printf("TODO NOQUESA: %s\n", __func__);
#if 0	// NOQUESA
TQ3CameraData					myCameraData;
TQ3ViewAngleAspectCameraData	myViewAngleCameraData;
TQ3Area							pane;
TQ3Status						status;
QD3DCameraDefType 				*cameraDefPtr;

	cameraDefPtr = &setupDefPtr->camera;

		/* GET PANE */
		//
		// Note: Q3DrawContext_GetPane seems to return garbage on pixmaps so, rig it.
		//
		
	if (setupDefPtr->view.useWindow)
	{
		status = Q3DrawContext_GetPane(gQD3D_DrawContext,&pane);				// get window pane info
		GAME_ASSERT(status);
	}
	else
	{
		Rect	r;
		
		GetPortBounds(setupDefPtr->view.gworld, &r);
		
		pane.min.x = pane.min.y = 0;
		pane.max.x = r.right;
		pane.max.y = r.bottom;
	}


				/* FILL IN CAMERA DATA */
				
	myCameraData.placement.cameraLocation = cameraDefPtr->from;			// set camera coords
	myCameraData.placement.pointOfInterest = cameraDefPtr->to;			// set target coords
	myCameraData.placement.upVector = cameraDefPtr->up;					// set a vector that's "up"
	myCameraData.range.hither = cameraDefPtr->hither;					// set frontmost Z dist
	myCameraData.range.yon = cameraDefPtr->yon;							// set farthest Z dist
	myCameraData.viewPort.origin.x = -1.0;								// set view origins?
	myCameraData.viewPort.origin.y = 1.0;
	myCameraData.viewPort.width = 2.0;
	myCameraData.viewPort.height = 2.0;

	myViewAngleCameraData.cameraData = myCameraData;
	myViewAngleCameraData.fov = cameraDefPtr->fov;						// larger = more fisheyed
	myViewAngleCameraData.aspectRatioXToY =
				(pane.max.x-pane.min.x)/(pane.max.y-pane.min.y);

	gQD3D_CameraObject = Q3ViewAngleAspectCamera_New(&myViewAngleCameraData);	 // create new camera
	GAME_ASSERT(gQD3D_CameraObject);

	status = Q3View_SetCamera(gQD3D_ViewObject, gQD3D_CameraObject);		// assign camera to view
	GAME_ASSERT(status);
#endif
}


/********************* CREATE LIGHTS ************************/

static void CreateLights(QD3DLightDefType *lightDefPtr)
{
			/************************/
			/* CREATE AMBIENT LIGHT */
			/************************/

	if (lightDefPtr->ambientBrightness != 0)						// see if ambient exists
	{
		GLfloat ambient[4] =
		{
			lightDefPtr->ambientBrightness * lightDefPtr->ambientColor.r,
			lightDefPtr->ambientBrightness * lightDefPtr->ambientColor.g,
			lightDefPtr->ambientBrightness * lightDefPtr->ambientColor.b,
			1
		};
		glLightModelfv(GL_LIGHT_MODEL_AMBIENT, ambient);
	}

			/**********************/
			/* CREATE FILL LIGHTS */
			/**********************/

	for (int i = 0; i < lightDefPtr->numFillLights; i++)
	{
		static GLfloat lightamb[4] = { 0.0, 0.0, 0.0, 1.0 };
		GLfloat lightVec[4];
		GLfloat	diffuse[4];

					/* SET FILL DIRECTION */

		Q3Vector3D_Normalize(&lightDefPtr->fillDirection[i], &lightDefPtr->fillDirection[i]);
		lightVec[0] = -lightDefPtr->fillDirection[i].x;		// negate vector because OGL is stupid
		lightVec[1] = -lightDefPtr->fillDirection[i].y;
		lightVec[2] = -lightDefPtr->fillDirection[i].z;
		lightVec[3] = 0;									// when w==0, this is a directional light, if 1 then point light
		glLightfv(GL_LIGHT0+i, GL_POSITION, lightVec);

					/* SET COLOR */

		glLightfv(GL_LIGHT0+i, GL_AMBIENT, lightamb);

		diffuse[0] = lightDefPtr->fillColor[i].r * lightDefPtr->fillBrightness[i];
		diffuse[1] = lightDefPtr->fillColor[i].g * lightDefPtr->fillBrightness[i];
		diffuse[2] = lightDefPtr->fillColor[i].b * lightDefPtr->fillBrightness[i];
		diffuse[3] = 1;

		glLightfv(GL_LIGHT0+i, GL_DIFFUSE, diffuse);

		glEnable(GL_LIGHT0+i);								// enable the light
	}
}



/******************** QD3D CHANGE DRAW SIZE *********************/
//
// Changes size of stuff to fit new window size and/or shink factor
//

void QD3D_ChangeDrawSize(QD3DSetupOutputType *setupInfo)
{
	printf("TODO NOQUESA: %s\n", __func__);
#if 0	// NOQUESA
Rect			r;
TQ3Area			pane;
TQ3ViewAngleAspectCameraData	cameraData;
TQ3Status		status;

			/* CHANGE DRAW CONTEXT PANE SIZE */
			
	if (setupInfo->window == nil)
		return;
		
	GetPortBounds(GetWindowPort(setupInfo->window), &r);										// get size of window
	pane.min.x = setupInfo->paneClip.left;													// set pane size
	pane.max.x = r.right-setupInfo->paneClip.right;
	pane.min.y = setupInfo->paneClip.top;
	pane.max.y = r.bottom-setupInfo->paneClip.bottom;

	status = Q3DrawContext_SetPane(setupInfo->drawContext,&pane);							// update pane in draw context
	GAME_ASSERT(status);

				/* CHANGE CAMERA ASPECT RATIO */
				
	status = Q3ViewAngleAspectCamera_GetData(setupInfo->cameraObject,&cameraData);			// get camera data
	GAME_ASSERT(status);

	cameraData.aspectRatioXToY = (pane.max.x-pane.min.x)/(pane.max.y-pane.min.y);			// set new aspect ratio
	status = Q3ViewAngleAspectCamera_SetData(setupInfo->cameraObject,&cameraData);			// set new camera data
	GAME_ASSERT(status);
#endif
}


/******************* QD3D DRAW SCENE *********************/

void QD3D_DrawScene(QD3DSetupOutputType *setupInfo, void (*drawRoutine)(const QD3DSetupOutputType *))
{
	GAME_ASSERT(setupInfo);
	GAME_ASSERT(setupInfo->isActive);									// make sure it's legit

			/* START RENDERING */

	int mkc = SDL_GL_MakeCurrent(gSDLWindow, gGLContext);
	GAME_ASSERT_MESSAGE(mkc == 0, SDL_GetError());

	Render_StartFrame();

			/* SET UP SCISSOR TEST */

	if (setupInfo->needScissorTest)
	{
		// Set scissor
		TQ3Area pane	= Render_GetAdjustedViewportRect(setupInfo->paneClip, GAME_VIEW_WIDTH, GAME_VIEW_HEIGHT);
		int paneWidth	= pane.max.x-pane.min.x;
		int paneHeight	= pane.max.y-pane.min.y;
		setupInfo->aspectRatio = paneWidth / (paneHeight + .001f);
		Render_SetViewport(true, pane.min.x, gWindowHeight-pane.max.y, paneWidth, paneHeight);
	}
	else
	{
		setupInfo->aspectRatio = gWindowWidth / (gWindowHeight + .001f);
		Render_SetViewport(false, 0, 0, gWindowWidth, gWindowHeight);
	}

			/* SET UP CAMERA */

	CalcCameraMatrixInfo(setupInfo);						// update camera matrix


			/* SOURCE PORT STUFF */

//if (gQD3D_FreshDrawContext)
//{
//	SDL_GL_SetSwapInterval(gGamePrefs.vsync ? 1 : 0);

//	Overlay_Alloc();									// source port addition (must be after StartRendering so we have a valid GL context)

//	gQD3D_FreshDrawContext = false;
//}
//
//if (setupInfo->enableMultisamplingByDefault)
//{
//	QD3D_SetMultisampling(true);
//}



			/***************/
			/* RENDER LOOP */
			/***************/
#if 0	// NOQUESA
	do
	{
				/* DRAW STYLES */

		QD3D_ReEnableFog(setupInfo);
				
		myStatus = Q3Style_Submit(setupInfo->interpolationStyle,setupInfo->viewObject);
		GAME_ASSERT(myStatus);

		myStatus = Q3Style_Submit(setupInfo->backfacingStyle,setupInfo->viewObject);
		GAME_ASSERT(myStatus);
			
		myStatus = Q3Style_Submit(setupInfo->fillStyle, setupInfo->viewObject);
		GAME_ASSERT(myStatus);

		myStatus = Q3Shader_Submit(setupInfo->shaderObject, setupInfo->viewObject);
		GAME_ASSERT(myStatus);

			/* DRAW NORMAL */
			
		if (gShowDebug)
			DrawNormal(setupInfo->viewObject);

			/* CALL INPUT DRAW FUNCTION */

		if (drawRoutine != nil)
			drawRoutine(setupInfo);

		myViewStatus = Q3View_EndRendering(setupInfo->viewObject);
		GAME_ASSERT(myViewStatus != kQ3ViewStatusError);

	} while ( myViewStatus == kQ3ViewStatusRetraverse );	
#endif

	if (drawRoutine)
		drawRoutine(setupInfo);
	
	QD3D_SetMultisampling(false);


			/******************/
			/* DONE RENDERING */
			/*****************/

	Render_EndFrame();

	SubmitInfobarOverlay();			// draw 2D elements on top

	// TODO: draw fade overlay here

	SDL_GL_SwapWindow(gSDLWindow);
}


//=======================================================================================================
//=============================== CAMERA STUFF ==========================================================
//=======================================================================================================

#pragma mark -

/*************** QD3D_UpdateCameraFromTo ***************/

void QD3D_UpdateCameraFromTo(QD3DSetupOutputType *setupInfo, TQ3Point3D *from, TQ3Point3D *to)
{
	setupInfo->currentCameraCoords = *from;					// set camera coords
	setupInfo->currentCameraLookAt = *to;					// set camera look at
#if 0	// NOQUESA
TQ3Status	status;
TQ3CameraPlacement	placement;
TQ3CameraObject		camera;

			/* GET CURRENT CAMERA INFO */

	camera = setupInfo->cameraObject;
			
	status = Q3Camera_GetPlacement(camera, &placement);
	GAME_ASSERT(status);


			/* SET CAMERA LOOK AT */
			
	placement.pointOfInterest = *to;
	setupInfo->currentCameraLookAt = *to;


			/* SET CAMERA COORDS */
			
	placement.cameraLocation = *from;
	setupInfo->currentCameraCoords = *from;				// keep global copy for quick use


			/* UPDATE CAMERA INFO */
			
	status = Q3Camera_SetPlacement(camera, &placement);
	GAME_ASSERT(status);
		
#endif
	UpdateListenerLocation();
}


/*************** QD3D_UpdateCameraFrom ***************/

void QD3D_UpdateCameraFrom(QD3DSetupOutputType *setupInfo, TQ3Point3D *from)
{
	setupInfo->currentCameraCoords = *from;					// set camera coords
#if 0	// NOQUESA
TQ3Status	status;
TQ3CameraPlacement	placement;

			/* GET CURRENT CAMERA INFO */
			
	status = Q3Camera_GetPlacement(setupInfo->cameraObject, &placement);
	GAME_ASSERT(status);


			/* SET CAMERA COORDS */
			
	placement.cameraLocation = *from;
	setupInfo->currentCameraCoords = *from;				// keep global copy for quick use
	

			/* UPDATE CAMERA INFO */
			
	status = Q3Camera_SetPlacement(setupInfo->cameraObject, &placement);
	GAME_ASSERT(status);
#endif
	UpdateListenerLocation();
}


/*************** QD3D_MoveCameraFromTo ***************/

void QD3D_MoveCameraFromTo(QD3DSetupOutputType *setupInfo, TQ3Vector3D *moveVector, TQ3Vector3D *lookAtVector)
{
	setupInfo->currentCameraCoords.x += moveVector->x;		// set camera coords
	setupInfo->currentCameraCoords.y += moveVector->y;
	setupInfo->currentCameraCoords.z += moveVector->z;

	setupInfo->currentCameraLookAt.x += lookAtVector->x;	// set camera look at
	setupInfo->currentCameraLookAt.y += lookAtVector->y;
	setupInfo->currentCameraLookAt.z += lookAtVector->z;
#if 0	// NOQUESA
TQ3Status	status;
TQ3CameraPlacement	placement;

			/* GET CURRENT CAMERA INFO */
			
	status = Q3Camera_GetPlacement(setupInfo->cameraObject, &placement);
	GAME_ASSERT(status);


			/* SET CAMERA COORDS */
			

	placement.cameraLocation.x += moveVector->x;
	placement.cameraLocation.y += moveVector->y;
	placement.cameraLocation.z += moveVector->z;

	placement.pointOfInterest.x += lookAtVector->x;
	placement.pointOfInterest.y += lookAtVector->y;
	placement.pointOfInterest.z += lookAtVector->z;
	
	setupInfo->currentCameraCoords = placement.cameraLocation;	// keep global copy for quick use
	setupInfo->currentCameraLookAt = placement.pointOfInterest;

			/* UPDATE CAMERA INFO */
			
	status = Q3Camera_SetPlacement(setupInfo->cameraObject, &placement);
	GAME_ASSERT(status);
#endif
	UpdateListenerLocation();
}


//=======================================================================================================
//=============================== LIGHTS STUFF ==========================================================
//=======================================================================================================

#pragma mark -


/********************* QD3D ADD POINT LIGHT ************************/

TQ3GroupPosition QD3D_AddPointLight(QD3DSetupOutputType *setupInfo,TQ3Point3D *point, TQ3ColorRGB *color, float brightness)
{
	printf("TODO NOQUESA: %s\n", __func__);
	return 0;
#if 0	// NOQUESA
TQ3GroupPosition		myGroupPosition;
TQ3LightData			myLightData;
TQ3PointLightData		myPointLightData;
TQ3LightObject			myLight;


	myLightData.isOn = kQ3True;											// light is ON
	
	myLightData.color = *color;											// set color of light
	myLightData.brightness = brightness;								// set brightness
	myPointLightData.lightData = myLightData;							// refer to general light info
	myPointLightData.castsShadows = kQ3False;							// no shadows
	myPointLightData.location = *point;									// set coords
	
	myPointLightData.attenuation = kQ3AttenuationTypeNone;				// set attenuation
	myLight = Q3PointLight_New(&myPointLightData);						// make it
	GAME_ASSERT(myLight);

	myGroupPosition = (TQ3GroupPosition)Q3Group_AddObject(setupInfo->lightGroup, myLight);// add to light group
	GAME_ASSERT(myGroupPosition);

	Q3Object_Dispose(myLight);											// dispose of light
	return(myGroupPosition);
#endif
}


/****************** QD3D SET POINT LIGHT COORDS ********************/

void QD3D_SetPointLightCoords(QD3DSetupOutputType *setupInfo, TQ3GroupPosition lightPosition, TQ3Point3D *point)
{
	printf("TODO NOQUESA: %s\n", __func__);
#if 0	// NOQUESA
TQ3PointLightData	pointLightData;
TQ3LightObject		light;
TQ3Status			status;

	status = Q3Group_GetPositionObject(setupInfo->lightGroup, lightPosition, &light);	// get point light object from light group
	GAME_ASSERT(status);

	status =  Q3PointLight_GetData(light, &pointLightData);				// get light data
	GAME_ASSERT(status);

	pointLightData.location = *point;									// set coords

	status = Q3PointLight_SetData(light, &pointLightData);				// update light data
	GAME_ASSERT(status);
		
	Q3Object_Dispose(light);
#endif
}


/****************** QD3D SET POINT LIGHT BRIGHTNESS ********************/

void QD3D_SetPointLightBrightness(QD3DSetupOutputType *setupInfo, TQ3GroupPosition lightPosition, float bright)
{
	printf("TODO NOQUESA: %s\n", __func__);
#if 0	// NOQUESA
TQ3LightObject		light;
TQ3Status			status;

	status = Q3Group_GetPositionObject(setupInfo->lightGroup, lightPosition, &light);	// get point light object from light group
	GAME_ASSERT(status);

	status = Q3Light_SetBrightness(light, bright);
	GAME_ASSERT(status);

	Q3Object_Dispose(light);
#endif
}



/********************* QD3D ADD FILL LIGHT ************************/

TQ3GroupPosition QD3D_AddFillLight(QD3DSetupOutputType *setupInfo,TQ3Vector3D *fillVector, TQ3ColorRGB *color, float brightness)
{
	printf("TODO NOQUESA: %s\n", __func__);
	return nil;
#if 0	// NOQUESA
TQ3GroupPosition		myGroupPosition;
TQ3LightData			myLightData;
TQ3LightObject			myLight;
TQ3DirectionalLightData	myDirectionalLightData;


	myLightData.isOn = kQ3True;									// light is ON
	
	myLightData.color = *color;									// set color of light
	myLightData.brightness = brightness;						// set brightness
	myDirectionalLightData.lightData = myLightData;				// refer to general light info
	myDirectionalLightData.castsShadows = kQ3False;				// no shadows
	myDirectionalLightData.direction = *fillVector;				// set vector
	
	myLight = Q3DirectionalLight_New(&myDirectionalLightData);	// make it
	GAME_ASSERT(myLight);

	myGroupPosition = (TQ3GroupPosition)Q3Group_AddObject(setupInfo->lightGroup, myLight);	// add to light group
	GAME_ASSERT(myGroupPosition != 0);

	Q3Object_Dispose(myLight);												// dispose of light
	return(myGroupPosition);
#endif
}

/********************* QD3D ADD AMBIENT LIGHT ************************/

TQ3GroupPosition QD3D_AddAmbientLight(QD3DSetupOutputType *setupInfo, TQ3ColorRGB *color, float brightness)
{
	printf("TODO NOQUESA: %s\n", __func__);
	return nil;
#if 0	// NOQUESA
TQ3GroupPosition		myGroupPosition;
TQ3LightData			myLightData;
TQ3LightObject			myLight;



	myLightData.isOn = kQ3True;									// light is ON
	myLightData.color = *color;									// set color of light
	myLightData.brightness = brightness;						// set brightness
	
	myLight = Q3AmbientLight_New(&myLightData);					// make it
	GAME_ASSERT(myLight);

	myGroupPosition = (TQ3GroupPosition)Q3Group_AddObject(setupInfo->lightGroup, myLight);		// add to light group
	GAME_ASSERT(myGroupPosition != 0);

	Q3Object_Dispose(myLight);									// dispose of light
	
	return(myGroupPosition);
#endif
}




/****************** QD3D DELETE LIGHT ********************/

void QD3D_DeleteLight(QD3DSetupOutputType *setupInfo, TQ3GroupPosition lightPosition)
{
	printf("TODO NOQUESA: %s\n", __func__);
#if 0	// NOQUESA
TQ3LightObject		light;

	light = (TQ3LightObject)Q3Group_RemovePosition(setupInfo->lightGroup, lightPosition);
	GAME_ASSERT(light);

	Q3Object_Dispose(light);
#endif
}


/****************** QD3D DELETE ALL LIGHTS ********************/
//
// Deletes ALL lights from the light group, including the ambient light.
//

void QD3D_DeleteAllLights(QD3DSetupOutputType *setupInfo)
{
	printf("TODO NOQUESA: %s\n", __func__);
#if 0	// NOQUESA
TQ3Status				status;

	status = Q3Group_EmptyObjects(setupInfo->lightGroup);
	GAME_ASSERT(status);
#endif
}




//=======================================================================================================
//=============================== TEXTURE MAP STUFF =====================================================
//=======================================================================================================

#pragma mark -

/**************** QD3D GET TEXTURE MAP ***********************/
//
// Loads a PICT resource and returns a shader object which is
// based on the PICT converted to a texture map.
//
// INPUT: textureRezID = resource ID of texture PICT to get.
//			blackIsAlpha = true if want to turn alpha on and to scan image for alpha pixels
//
// OUTPUT: TQ3ShaderObject = shader object for texture map.  nil == error.
//

GLuint QD3D_NumberedTGAToTexture(long textureRezID, bool blackIsAlpha, int flags)
{
	char path[128];
	FSSpec spec;

	snprintf(path, sizeof(path), ":Images:Textures:%ld.tga", textureRezID);

	FSMakeFSSpec(gDataSpec.vRefNum, gDataSpec.parID, path, &spec);

	return QD3D_TGAToTexture(&spec, blackIsAlpha, flags);
}

GLuint QD3D_TGAToTexture(FSSpec* spec, bool blackIsAlpha, int flags)
{
uint8_t*				pixelData = nil;
TGAHeader				header;
OSErr					err;

	err = ReadTGA(spec, &pixelData, &header, false);
	if (err != noErr)
	{
		return 0;
	}

	GAME_ASSERT(header.bpp == 24 || header.bpp == 16);
	GAME_ASSERT(header.imageType == TGA_IMAGETYPE_RAW_RGB);

	if (blackIsAlpha)
	{
		printf("TODO NOQUESA: Black is alpha\n");
	}

	GLuint glTextureName = Render_LoadTexture(
			GL_RGB,
			header.width,
			header.height,
			header.bpp == 16 ? GL_BGRA : GL_BGR,
			header.bpp == 16 ? GL_UNSIGNED_SHORT_1_5_5_5_REV : GL_UNSIGNED_BYTE,
			pixelData,
			flags
			);

	DisposePtr((Ptr) pixelData);

	return glTextureName;
}

/******************** DRAW PICT INTO MIPMAP ********************/
//
// OUTPUT: mipmap = new mipmap holding texture image
//

static void DrawPICTIntoMipmap(PicHandle pict,long width, long height, TQ3Mipmap *mipmap, Boolean blackIsAlpha)
{
	printf("TODO NOQUESA: %s\n", __func__);
#if 0	// NOQUESA
	GAME_ASSERT(width  == (**pict).picFrame.right  - (**pict).picFrame.left);
	GAME_ASSERT(height == (**pict).picFrame.bottom - (**pict).picFrame.top);
	
	Ptr pictMapAddr = (**pict).__pomme_pixelsARGB32;
	long pictRowBytes = width * (32 / 8);

	if (blackIsAlpha)
	{
		const uint32_t alphaMask = 0x000000FF;

		// First clear black areas
		uint32_t*	rowPtr = (uint32_t *)pictMapAddr;

		for (int y = 0; y < height; y++)
		{
			for (int x = 0; x < width; x++)
			{
				uint32_t pixel = rowPtr[x];
				if (!(pixel & ~alphaMask))
					rowPtr[x] = 0;
			}
			rowPtr += pictRowBytes / 4;
		}
	}
	
	mipmap->image = Q3MemoryStorage_New((unsigned char*)pictMapAddr, pictRowBytes * height);
	GAME_ASSERT(mipmap->image);

	mipmap->useMipmapping = kQ3False;							// not actually using mipmaps (just 1 srcmap)
	mipmap->pixelType = kQ3PixelTypeARGB32;						// if 32bit, assume alpha

	mipmap->bitOrder = kQ3EndianBig;
	mipmap->byteOrder = kQ3EndianBig;
	mipmap->reserved = 0;
	mipmap->mipmaps[0].width = width;
	mipmap->mipmaps[0].height = height;
	mipmap->mipmaps[0].rowBytes = pictRowBytes;
	mipmap->mipmaps[0].offset = 0;

	if (blackIsAlpha)											// apply edge padding to texture to avoid black seams
	{															// where texels are being discarded
		ApplyEdgePadding(mipmap);
	}
#endif
}


/**************** QD3D: DATA16 TO TEXTURE_NOMIP ***********************/
//
// Converts input data to non mipmapped texture
//
// INPUT: .
//
// OUTPUT: TQ3ShaderObject = shader object for texture map.
//

TQ3SurfaceShaderObject	QD3D_Data16ToTexture_NoMip(Ptr data, short width, short height)
{
	printf("TODO NOQUESA: %s\n", __func__);
	return nil;
#if 0	// NOQUESA
TQ3Mipmap 					mipmap;
TQ3TextureObject			texture;
TQ3SurfaceShaderObject		shader;

			/* CREATE MIPMAP */
			
	Data16ToMipmap(data,width,height,&mipmap);


			/* MAKE NEW MIPMAP TEXTURE */
			
	texture = Q3MipmapTexture_New(&mipmap);							// make new mipmap	
	GAME_ASSERT(texture);
			
	shader = Q3TextureShader_New(texture);
	GAME_ASSERT(shader);

	Q3Object_Dispose (texture);
	Q3Object_Dispose (mipmap.image);					// dispose of extra ref to storage object

	return(shader);
#endif
}


/******************** DATA16 TO MIPMAP ********************/
//
// Creates a mipmap from an existing 16bit data buffer (note that buffer is not copied!)
//
// OUTPUT: mipmap = new mipmap holding texture image
//

static void Data16ToMipmap(Ptr data, short width, short height, TQ3Mipmap *mipmap)
{
	printf("TODO NOQUESA: %s\n", __func__);
#if 0	// NOQUESA
long	size = width * height * 2;

			/* MAKE 16bit MIPMAP */

	mipmap->image = (void *)Q3MemoryStorage_NewBuffer ((unsigned char *) data, size,size);
	if (mipmap->image == nil)
	{
		DoAlert("Data16ToMipmap: Q3MemoryStorage_New Failed!");
		QD3D_ShowRecentError();
	}

	mipmap->useMipmapping 	= kQ3False;							// not actually using mipmaps (just 1 srcmap)
	mipmap->pixelType 		= kQ3PixelTypeRGB16;						
	mipmap->bitOrder 		= kQ3EndianBig;

	// Source port note: these images come from 'Timg' resources read in File.c.
	// File.c byteswaps the entire Timg, so they're little-endian now.
	mipmap->byteOrder 		= kQ3EndianLittle;

	mipmap->reserved 			= 0;
	mipmap->mipmaps[0].width 	= width;
	mipmap->mipmaps[0].height 	= height;
	mipmap->mipmaps[0].rowBytes = width*2;
	mipmap->mipmaps[0].offset 	= 0;
#endif
}


/**************** QD3D: GET MIPMAP STORAGE OBJECT FROM ATTRIB **************************/
//
// NOTE: the mipmap.image and surfaceShader are *valid* references which need to be de-referenced later!!!
//
// INPUT: attribSet
//
// OUTPUT: surfaceShader = shader extracted from attribute set
//

TQ3StorageObject QD3D_GetMipmapStorageObjectFromAttrib(TQ3AttributeSet attribSet)
{
	printf("TODO NOQUESA: %s\n", __func__);
	return nil;
#if 0	// NOQUESA
TQ3Status	status;

TQ3TextureObject		texture;
TQ3Mipmap 				mipmap;
TQ3StorageObject		storage;
TQ3SurfaceShaderObject	surfaceShader;

			/* GET SHADER FROM ATTRIB */
			
	status = Q3AttributeSet_Get(attribSet, kQ3AttributeTypeSurfaceShader, &surfaceShader);
	GAME_ASSERT(status);

			/* GET TEXTURE */
			
	status = Q3TextureShader_GetTexture(surfaceShader, &texture);
	GAME_ASSERT(status);

			/* GET MIPMAP */
			
	status = Q3MipmapTexture_GetMipmap(texture,&mipmap);
	GAME_ASSERT(status);

		/* GET A LEGAL REF TO STORAGE OBJ */
			
	storage = mipmap.image;
	
			/* DISPOSE REFS */
			
	Q3Object_Dispose(texture);	
	Q3Object_Dispose(surfaceShader);
	return(storage);
#endif
}


#pragma mark -

//=======================================================================================================
//=============================== STYLE STUFF =====================================================
//=======================================================================================================


/****************** QD3D:  SET BACKFACE STYLE ***********************/

void SetBackFaceStyle(QD3DSetupOutputType *setupInfo, TQ3BackfacingStyle style)
{
	printf("TODO NOQUESA: %s\n", __func__);
#if 0	// NOQUESA
TQ3Status status;
TQ3BackfacingStyle	backfacingStyle;

	status = Q3BackfacingStyle_Get(setupInfo->backfacingStyle, &backfacingStyle);
	GAME_ASSERT(status);

	if (style == backfacingStyle)							// see if already set to that
		return;
		
	status = Q3BackfacingStyle_Set(setupInfo->backfacingStyle, style);
	GAME_ASSERT(status);
#endif

}


/****************** QD3D:  SET FILL STYLE ***********************/

void SetFillStyle(QD3DSetupOutputType *setupInfo, TQ3FillStyle style)
{
	printf("TODO NOQUESA: %s\n", __func__);
#if 0	// NOQUESA
TQ3Status 		status;
TQ3FillStyle	fillStyle;

	status = Q3FillStyle_Get(setupInfo->fillStyle, &fillStyle);
	GAME_ASSERT(status);

	if (style == fillStyle)							// see if already set to that
		return;
		
	status = Q3FillStyle_Set(setupInfo->fillStyle, style);
	GAME_ASSERT(status);
#endif

}


//=======================================================================================================
//=============================== MISC ==================================================================
//=======================================================================================================

/************** QD3D CALC FRAMES PER SECOND *****************/

void	QD3D_CalcFramesPerSecond(void)
{
UnsignedWide	wide;
unsigned long	now;
static	unsigned long then = 0;


			/* DO REGULAR CALCULATION */
			
	Microseconds(&wide);
	now = wide.lo;
	if (then != 0)
	{
		gFramesPerSecond = 1000000.0f/(float)(now-then);
		if (gFramesPerSecond < DEFAULT_FPS)			// (avoid divide by 0's later)
			gFramesPerSecond = DEFAULT_FPS;

		if (gFramesPerSecond < 9.0f)					// this is the minimum we let it go
			gFramesPerSecond = 9.0f;
		
	}
	else
		gFramesPerSecond = DEFAULT_FPS;
		
//	gFramesPerSecondFrac = 1/gFramesPerSecond;		// calc fractional for multiplication
	gFramesPerSecondFrac = __fres(gFramesPerSecond);	
	
	then = now;										// remember time	
}


#pragma mark -

/************ QD3D: SHOW RECENT ERROR *******************/

void QD3D_ShowRecentError(void)
{
	printf("TODO NOQUESA: %s\n", __func__);
#if 0	// NOQUESA
TQ3Error	q3Err;
Str255		s;
	
	q3Err = Q3Error_Get(nil);
	if (q3Err == kQ3ErrorOutOfMemory)
		DoFatalAlert("QuickDraw 3D has run out of memory!");
	else
	if (q3Err == kQ3ErrorMacintoshError)
		DoFatalAlert("kQ3ErrorMacintoshError");
	else
	if (q3Err == kQ3ErrorNotInitialized)
		DoFatalAlert("kQ3ErrorNotInitialized");
	else
	if (q3Err == kQ3ErrorReadLessThanSize)
		DoFatalAlert("kQ3ErrorReadLessThanSize");
	else
	if (q3Err == kQ3ErrorViewNotStarted)
		DoFatalAlert("kQ3ErrorViewNotStarted");
	else
	if (q3Err != 0)
	{
		snprintf(s, sizeof(s), "QD3D Error %d\nLook up error code in QuesaErrors.h", q3Err);
		DoFatalAlert(s);
	}
#endif
}


#pragma mark -



/************************ SET TRIANGLE CACHE MODE *****************************/
//
// For ATI driver, sets triangle caching flag for xparent triangles
//

void QD3D_SetTriangleCacheMode(Boolean isOn)
{
#if 0	// Source port removal. We'd need to extend Quesa for this.
	GAME_ASSERT(gQD3D_DrawContext);

		QASetInt(gQD3D_DrawContext, kQATag_ZSortedHint, isOn);
#endif
}	
				
/************************ SET Z WRITE *****************************/
//
// For ATI driver, turns on/off z-buffer writes
// QASetInt(gQD3D_DrawContext, kQATag_ZBufferMask, isOn)
// (Source port note: added Quesa extension for this)
//

void QD3D_SetZWrite(Boolean isOn)
{
	printf("TODO NOQUESA: %s\n", __func__);
#if 0	// NOQUESA
	if (!gQD3D_DrawContext)
		return;

	TQ3Status status = Q3ZWriteTransparencyStyle_Submit(isOn ? kQ3On : kQ3Off, gQD3D_ViewObject);
	GAME_ASSERT(status);
#endif
}	


/************************ SET BLENDING MODE ************************/

void QD3D_SetAdditiveBlending(Boolean enable)
{
	printf("TODO NOQUESA: %s\n", __func__);
#if 0	// NOQUESA
	static const TQ3BlendingStyleData normalStyle	= { kQ3Off, GL_ONE, GL_ONE_MINUS_SRC_ALPHA };
	static const TQ3BlendingStyleData additiveStyle	= { kQ3On, GL_ONE, GL_ONE };

	GAME_ASSERT(gQD3D_ViewObject);

	Q3BlendingStyle_Submit(enable ? &additiveStyle : &normalStyle, gQD3D_ViewObject);
#endif
}

/************************ SET MULTISAMPLING ************************/

void QD3D_SetMultisampling(Boolean enable)
{
#if ALLOW_MSAA
	static bool multisamplingEnabled = false;
	
	if (multisamplingEnabled == enable)
	{
		// no-op
	}
	else if (!enable && multisamplingEnabled)			// If we want to disable, always do it if MSAA was currently active
	{
		glDisable(GL_MULTISAMPLE);
		multisamplingEnabled = false;
	}
	else if (gGamePrefs.antiAliasing)				// otherwise only honor request if prefs allow MSAA
	{
		if (enable)
			glEnable(GL_MULTISAMPLE);
		else
			glDisable(GL_MULTISAMPLE);
		multisamplingEnabled = enable;
	}
#endif
}


#pragma mark -

/********************* SHOW NORMAL **************************/

void ShowNormal(TQ3Point3D *where, TQ3Vector3D *normal)
{
	gNormalWhere = *where;
	gNormal = *normal;

}

/********************* DRAW NORMAL **************************/

static void DrawNormal(TQ3ViewObject view)
{
	printf("TODO NOQUESA: %s\n", __func__);
#if 0	// NOQUESA
TQ3LineData	line;

	line.lineAttributeSet = nil;

	line.vertices[0].attributeSet = nil;
	line.vertices[0].point = gNormalWhere;

	line.vertices[1].attributeSet = nil;
	line.vertices[1].point.x = gNormalWhere.x + gNormal.x * 400.0f;
	line.vertices[1].point.y = gNormalWhere.y + gNormal.y * 400.0f;
	line.vertices[1].point.z = gNormalWhere.z + gNormal.z * 400.0f;

	Q3Line_Submit(&line, view);
#endif
}

