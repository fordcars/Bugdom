//
// camera.h
//

#define	HITHER_DISTANCE	20.0f
#ifdef __3DS__
    #define	YON_DISTANCE	2000.0f
#else
    #define	YON_DISTANCE	2500.0f
#endif

void InitCamera(void);
void UpdateCamera(void);
extern	void CalcCameraMatrixInfo(QD3DSetupOutputType *);
extern	void ResetCameraSettings(void);
void DrawLensFlare(const QD3DSetupOutputType *setupInfo);
void DisposeLensFlares(void);
