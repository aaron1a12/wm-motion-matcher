#pragma once

/*
Copyright 2021 Daniel Holden

Redistribution and use in source and binary forms, with or without modification,
are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this
list of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright notice, this
list of conditions and the following disclaimer in the documentation and/or other
materials provided with the distribution.

3. Neither the name of the copyright holder nor the names of its contributors may
be used to endorse or promote products derived from this software without
specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
OF THE POSSIBILITY OF SUCH DAMAGE.
*/

//--------------------------------------

float lerp(float x, float y, float a)
{
    return (1.0f - a) * x + a * y;
}

//--------------------------------------

float damper(float x, float g, float factor)
{
    return lerp(x, g, factor);
}

float damper_bad(float x, float g, float damping, float dt)
{
    return lerp(x, g, damping * dt);
}

float damper_exponential(
    float x,
    float g,
    float damping,
    float dt,
    float ft = 1.0f / 60.0f)
{
    return lerp(x, g, 1.0f - powf(1.0 / (1.0 - ft * damping), -dt / ft));
}


float fast_negexp(float x)
{
    return 1.0f / (1.0f + x + 0.48f * x * x + 0.235f * x * x * x);
}

float damper_implicit(float x, float g, float halflife, float dt, float eps = 1e-5f)
{
    return lerp(x, g, 1.0f - fast_negexp((0.69314718056f * dt) / (halflife + eps)));
}

//--------------------------------------

void spring_damper_bad(
    float& x,
    float& v,
    float g,
    float q,
    float stiffness,
    float damping,
    float dt)
{
    v += dt * stiffness * (g - x) + dt * damping * (q - v);
    x += dt * v;
}

float fast_atan(float x)
{
    float z = fabs(x);
    float w = z > 1.0f ? 1.0f / z : z;
    float y = (PI / 4.0f) * w - w * (w - 1) * (0.2447f + 0.0663f * w);
    return copysign(z > 1.0f ? PI / 2.0 - y : y, x);
}

float squaref(float x)
{
    return x * x;
}

float halflife_to_damping(float halflife, float eps = 1e-5f)
{
    return (4.0f * 0.69314718056f) / (halflife + eps);
}

float damping_to_halflife(float damping, float eps = 1e-5f)
{
    return (4.0f * 0.69314718056f) / (damping + eps);
}

float frequency_to_stiffness(float frequency)
{
    return squaref(2.0f * PI * frequency);
}

float stiffness_to_frequency(float stiffness)
{
    return sqrtf(stiffness) / (2.0f * PI);
}

float critical_halflife(float frequency)
{
    return damping_to_halflife(sqrtf(frequency_to_stiffness(frequency) * 4.0f));
}

float critical_frequency(float halflife)
{
    return stiffness_to_frequency(squaref(halflife_to_damping(halflife)) / 4.0f);
}

void spring_damper_implicit(
    float& x,
    float& v,
    float x_goal,
    float v_goal,
    float frequency,
    float halflife,
    float dt,
    float eps = 1e-5f)
{
    float g = x_goal;
    float q = v_goal;
    float s = frequency_to_stiffness(frequency);
    float d = halflife_to_damping(halflife);
    float c = g + (d * q) / (s + eps);
    float y = d / 2.0f;

    if (fabs(s - (d * d) / 4.0f) < eps) // Critically Damped
    {
        float j0 = x - c;
        float j1 = v + j0 * y;

        float eydt = fast_negexp(y * dt);

        x = j0 * eydt + dt * j1 * eydt + c;
        v = -y * j0 * eydt - y * dt * j1 * eydt + j1 * eydt;
    }
    else if (s - (d * d) / 4.0f > 0.0) // Under Damped
    {
        float w = sqrtf(s - (d * d) / 4.0f);
        float j = sqrtf(squaref(v + y * (x - c)) / (w * w + eps) + squaref(x - c));
        float p = fast_atan((v + (x - c) * y) / (-(x - c) * w + eps));

        j = (x - c) > 0.0f ? j : -j;

        float eydt = fast_negexp(y * dt);

        x = j * eydt * cosf(w * dt + p) + c;
        v = -y * j * eydt * cosf(w * dt + p) - w * j * eydt * sinf(w * dt + p);
    }
    else if (s - (d * d) / 4.0f < 0.0) // Over Damped
    {
        float y0 = (d + sqrtf(d * d - 4 * s)) / 2.0f;
        float y1 = (d - sqrtf(d * d - 4 * s)) / 2.0f;
        float j1 = (c * y0 - x * y0 - v) / (y1 - y0);
        float j0 = x - j1 - c;

        float ey0dt = fast_negexp(y0 * dt);
        float ey1dt = fast_negexp(y1 * dt);

        x = j0 * ey0dt + j1 * ey1dt + c;
        v = -y0 * j0 * ey0dt - y1 * j1 * ey1dt;
    }
}

//--------------------------------------

void critical_spring_damper_implicit(
    float& x,
    float& v,
    float x_goal,
    float v_goal,
    float halflife,
    float dt)
{
    float g = x_goal;
    float q = v_goal;
    float d = halflife_to_damping(halflife);
    float c = g + (d * q) / ((d * d) / 4.0f);
    float y = d / 2.0f;
    float j0 = x - c;
    float j1 = v + j0 * y;
    float eydt = fast_negexp(y * dt);

    x = eydt * (j0 + j1 * dt) + c;
    v = eydt * (v - j1 * y * dt);
}

void simple_spring_damper_implicit(
    float& x,
    float& v,
    float x_goal,
    float halflife,
    float dt)
{
    float y = halflife_to_damping(halflife) / 2.0f;
    float j0 = x - x_goal;
    float j1 = v + j0 * y;
    float eydt = fast_negexp(y * dt);

    x = eydt * (j0 + j1 * dt) + x_goal;
    v = eydt * (v - j1 * y * dt);
}

void decay_spring_damper_implicit(
    float& x,
    float& v,
    float halflife,
    float dt)
{
    float y = halflife_to_damping(halflife) / 2.0f;
    float j1 = v + x * y;
    float eydt = fast_negexp(y * dt);

    x = eydt * (x + j1 * dt);
    v = eydt * (v - j1 * y * dt);
}

//--------------------------------------


void spring_character_update(
    float& x,
    float& v,
    float& a,
    float v_goal,
    float halflife,
    float dt)
{
    float y = halflife_to_damping(halflife) / 2.0f;
    float j0 = v - v_goal;
    float j1 = a + j0 * y;
    float eydt = fast_negexp(y * dt);

    x = eydt * ((j1 * dt) / y) + dt * v_goal + x;
    v = eydt * (j0 + j1 * dt) + v_goal;
    a = eydt * (a - j1 * y * dt);
}

void spring_character_predict(
    float px[],
    float pv[],
    float pa[],
    int count,
    float x,
    float v,
    float a,
    float v_goal,
    float halflife,
    float dt)
{
    for (int i = 0; i < count; i++)
    {
        px[i] = x;
        pv[i] = v;
        pa[i] = a;
    }

    for (int i = 0; i < count; i++)
    {
        spring_character_update(px[i], pv[i], pa[i], v_goal, halflife, i * dt);
    }
}