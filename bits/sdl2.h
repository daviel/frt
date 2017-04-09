// sdl2.h
/*
 * FRT - A Godot platform targeting single board computers
 * Copyright (c) 2017  Emanuele Fornara
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#include <stdlib.h>
#include <assert.h>

#include <SDL2/SDL.h>

/*
	WARNING: This is not a proper implementation!
	It is just a minimal version to help to test other modules on a typical
	UNIX setup.
 */

namespace frt {

class SDL2Context;

struct SDL2User {
private:
	friend SDL2Context;
	SDL2Context *ctx;
	const SDL_EventType *types;
	bool valid;
	SDL2User()
		: valid(false) {}

public:
	inline bool poll(SDL_Event *ev);
	inline void release();
};

class SDL2Context {
private:
	friend SDL2User;
	static const int max_users = 10;
	SDL2User users[max_users];
	int n_users;
	bool create_window;
	SDL_Window *window;
	SDL_Renderer *renderer;
	SDL2Context()
		: n_users(0), create_window(true), window(0), renderer(0) {
		SDL_Init(SDL_INIT_VIDEO);
	}
	~SDL2Context() {
		if (renderer)
			SDL_DestroyRenderer(renderer);
		if (window)
			SDL_DestroyWindow(window);
		SDL_Quit();
	}
	bool poll(const SDL_EventType *types, SDL_Event *event) {
		if (create_window && !window)
			SDL_CreateWindowAndRenderer(100, 100, 0, &window, &renderer);
		while (1) {
			SDL_PumpEvents();
			SDL_Event ev;
			if (SDL_PeepEvents(&ev, 1, SDL_PEEKEVENT, SDL_FIRSTEVENT,
			  SDL_LASTEVENT) == 0) {
				// no events: flush the screen
				if (window && renderer) {
					SDL_RenderClear(renderer);
					SDL_RenderPresent(renderer);
				}
				return false;
			}
			// is this event type handled by any user?
			bool user_event = false;
			for (int i = 0; i < max_users; i++) {
				if (!users[i].valid || !users[i].types)
					continue;
				const SDL_EventType *t = users[i].types;
				for (int j = 0; t[j] != SDL_LASTEVENT; j++)
					if (t[j] == ev.type)
						user_event = true;
			}
			// this user in particular?
			if (user_event && types) {
				for (int j = 0; types[j] != SDL_LASTEVENT; j++)
					if (types[j] == ev.type) {
						// ok: return it to the user
						SDL_PollEvent(&ev);
						if (event)
							*event = ev;
						return true;
					}
			}
			if (!user_event) {
				// no user is asking for this type of event: handle it here
				SDL_PollEvent(&ev);
				if (ev.type == SDL_QUIT)
					exit(0);
			} else {
				// some other user is waiting for this event...
				return false;
			}
		}
	}
	SDL2User *acquire_(const SDL_EventType *types, bool window_owner) {
		for (int i = 0; i < max_users; i++) {
			if (users[i].valid)
				continue;
			n_users++;
			users[i].valid = true;
			users[i].ctx = this;
			users[i].types = types;
			if (window_owner)
				create_window = false;
			return &users[i];
		}
		return 0;
	}
	void release(SDL2User *u) {
		u->valid = false;
		if (--n_users == 0) {
			delete this;
			App *app = App::instance();
			SDL2Context **ctx = (SDL2Context **)app->get_context("sdl2");
			*ctx = 0;
		}
	}

	/*
		Unfortunately, no RAII since users are driven by probe/cleanup.
		Be careful: no safeguards here!
	 */
public:
	static SDL2User *acquire(const SDL_EventType *types, bool window_owner = false) {
		App *app = App::instance();
		SDL2Context **ctx = (SDL2Context **)app->get_context("sdl2");
		if (!*ctx)
			*ctx = new SDL2Context();
		return (*ctx)->acquire_(types, window_owner);
	}
};

inline bool SDL2User::poll(SDL_Event *ev) {
	return ctx->poll(types, ev);
}

inline void SDL2User::release() {
	if (valid)
		ctx->release(this);
}

} // namespace frt