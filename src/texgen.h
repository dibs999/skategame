// Procedural texture generation: all surfaces are synthesized at startup.
#pragma once
#include <GL/glew.h>

struct TexSet {
    GLuint concrete = 0;      // weathered concrete w/ expansion joints + wax
    GLuint concreteBowl = 0;  // smoother skatepark concrete
    GLuint asphalt = 0;       // dark asphalt w/ painted parking lines
    GLuint wood = 0;          // wood grain planks (ramps)
    GLuint metal = 0;         // brushed metal (rails/trucks)
    GLuint grip = 0;          // black griptape
    GLuint deckArt = 0;       // board underside graphic
    GLuint denim = 0;         // jeans fabric
    GLuint fabric = 0;        // t-shirt jersey
    GLuint bark = 0;
    GLuint leaf = 0;
    GLuint curbPaint = 0;     // white/yellow painted concrete (curbs)
};

void generateAllTextures(TexSet& t);
