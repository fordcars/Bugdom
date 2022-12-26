//
// window.h
//

#ifdef __3DS__
    #define GAME_VIEW_WIDTH		(400)
    #define GAME_VIEW_HEIGHT	(240)
#else
    #define GAME_VIEW_WIDTH		(640)
    #define GAME_VIEW_HEIGHT	(480)
#endif


extern void	InitWindowStuff(void);
extern	void MakeFadeEvent(Boolean	fadeIn);

void GammaFadeOut(Boolean fadeSound);
extern	void GammaOn(void);

extern	void GameScreenToBlack(void);

void QD3D_OnWindowResized(int windowWidth, int windowHeight);
void SetFullscreenMode(void);

void DoSDLMaintenance(void);
