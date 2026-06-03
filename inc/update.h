#ifndef UPDATE_H
#define UPDATE_H

#ifdef __cplusplus
extern "C" {
#endif

void update(float dt);
void render(void);
int get_target_frame_ms(void);

#ifdef __cplusplus
}
#endif

#endif
