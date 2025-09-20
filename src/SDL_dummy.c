
#ifndef SDL_dummy_h_
#define SDL_dummy_h_

#include "SDL.h"

#define SDL_DUMMY(ret_type,name,params,ret_value) ret_type name##_REAL params { return ret_value; }

SDL_DUMMY(int,SDL_GL_LoadLibrary,(const char *a),-1)
SDL_DUMMY(void*,SDL_GL_GetProcAddress,(const char *a),NULL)
SDL_DUMMY(void,SDL_GL_UnloadLibrary,(void),)
SDL_DUMMY(SDL_bool,SDL_GL_ExtensionSupported,(const char *a),SDL_FALSE)
SDL_DUMMY(int,SDL_GL_SetAttribute,(SDL_GLattr a, int b),-1)
SDL_DUMMY(int,SDL_GL_GetAttribute,(SDL_GLattr a, int *b),-1)
SDL_DUMMY(SDL_GLContext,SDL_GL_CreateContext,(SDL_Window *a),NULL)
SDL_DUMMY(int,SDL_GL_MakeCurrent,(SDL_Window *a, SDL_GLContext b),-1)
SDL_DUMMY(SDL_Window*,SDL_GL_GetCurrentWindow,(void),NULL)
SDL_DUMMY(SDL_GLContext,SDL_GL_GetCurrentContext,(void),NULL)
SDL_DUMMY(void,SDL_GL_GetDrawableSize,(SDL_Window *a, int *b, int *c),)
SDL_DUMMY(int,SDL_GL_SetSwapInterval,(int a),-1)
SDL_DUMMY(int,SDL_GL_GetSwapInterval,(void),-1)
SDL_DUMMY(void,SDL_GL_SwapWindow,(SDL_Window *a),)
SDL_DUMMY(void,SDL_GL_DeleteContext,(SDL_GLContext a),)
SDL_DUMMY(void,SDL_GL_ResetAttributes,(void),)
SDL_DUMMY(int,SDL_GL_BindTexture,(SDL_Texture *a, float *b, float *c),-1)
SDL_DUMMY(int,SDL_GL_UnbindTexture,(SDL_Texture *a),-1)

#undef SDL_DUMMY

#endif // SDL_dummy_h_