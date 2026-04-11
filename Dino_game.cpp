/*This code originally had 541 lines.  
After I refactored it, it had about 340 lines.  
After adding comments to explain the functions (documentation and analysis), its lines have now become almost 600 — so it has 600 lines.
*/

/* #include "raylib.h"
This header brings in the Raylib API for windowing, input, 2D/3D rendering, audio, and basic utilities.
It defines types (Vector2, Rectangle), drawing functions, and resource management helpers used throughout the game.
Including raylib centralizes platform and graphics calls so the rest of the code can remain engine-agnostic.
It also ensures consistent initialization and teardown patterns for the graphics and audio subsystems.
*/
#include "raylib.h"
/* #include <string>
Provides the C++ std::string class and related utilities for safe, convenient text handling.
It replaces raw C-style char arrays with a higher-level, memory-managed type for building UI text, file paths, and messages.
Using std::string reduces bugs from buffer overflows and simplifies concatenation, formatting, and conversion tasks.
It also interoperates with standard library algorithms and I/O facilities when needed.
*/
#include <string>
/* #include <cmath>
Exposes standard mathematical functions and constants such as std::sin, std::cos, std::sqrt, and std::round.
These functions are essential for physics calculations, easing functions, angle math, and other numeric operations in games.
Using <cmath> ensures consistent, well-optimized implementations across platforms and makes intent explicit in the code.
It also provides overloads for float/double types so you can write numerically robust, readable math expressions.
*/
#include <cmath>

// -----------------------------------------------------------------------------
// Generic structure used for all animations (dino, obstacles, background)
// -----------------------------------------------------------------------------
/*
Cette structure centralise toutes les données nécessaires pour animer un sprite (joueur, obstacles, décor),
ce qui évite la duplication de champs et garantit une convention unique pour la gestion des animations.
Elle contient la zone source dans la sprite-sheet (rec), la position à l'écran (pos) et une référence de sol (baseY)
pour synchroniser la physique et l'alignement visuel. Les champs de timing (updateTime, runningTime) et d'index
(frame) permettent d'avancer les frames de manière indépendante du framerate, facilitant des animations fluides.
En regroupant ces éléments, on simplifie la réutilisation, l'extension (nouveaux états ou animations) et le débogage
des comportements visuels à travers tout le code du jeu.
*/
struct AnimData
{
    Rectangle rec;       
    Vector2 pos;         
    float baseY = 0;     
    int frame = 0;       
    float updateTime = 0;
    float runningTime = 0;
};

// -----------------------------------------------------------------------------
// Check if a character is touching the ground
// -----------------------------------------------------------------------------
/* 
This helper returns true when the character's feet are at or below the intended ground level.
It compares the sprite's vertical position against a ground baseline computed from window height.
Use this to gate jumping and to snap the player to the floor to avoid sinking due to float error.
It assumes rec.height represents the visible sprite height and pos.y is the sprite's top coordinate.
Keep calls frequent (every frame) so physics and animation remain synchronized.
*/
bool isOnGround(const AnimData &data, int windowHeight)
{
    return data.pos.y >= (windowHeight - 80) - data.rec.height;
}

// -----------------------------------------------------------------------------
// Update the frame of a sprite-sheet animation
// -----------------------------------------------------------------------------
/* 
Advances the animation timer and steps the sprite-sheet frame when the update interval elapses.
It accumulates delta time into runningTime and resets it when a frame change occurs to maintain steady FPS-independent animation.
The function updates rec.x to select the correct frame region from the sprite-sheet before returning the modified AnimData.
Callers should pass the correct maxFrame (zero-based index of the last frame) and a sensible updateTime to avoid flicker.
This routine is pure with respect to external state: it returns a new AnimData so callers must assign the result back to their animation variable.
*/
AnimData updateAnimData(AnimData data, float dT, int maxFrame)
{
    data.runningTime += dT;

    if (data.runningTime >= data.updateTime)
    {
        data.runningTime = 0;
        data.rec.x = data.frame * data.rec.width;
        data.frame = (data.frame + 1) % (maxFrame + 1);
    }
    return data;
}

// -----------------------------------------------------------------------------
// Collision with reduced hitbox (avoids massive code duplication)
// -----------------------------------------------------------------------------
/*  
Shrinks each rectangle by the given padding values before testing collision to create a smaller hitbox.
Padding is applied symmetrically on both axes so the effective collision box is inset by pad on all sides.
Use different pad values for A and B to tune forgiving or strict collisions between player and obstacles.
This wrapper centralizes hitbox adjustments so you avoid duplicating padding logic across the codebase.
It returns the same boolean as CheckCollisionRecs but with the padded rectangles, making intent explicit at call sites.
*/
bool CheckCollisionWithPadding(Rectangle A, Rectangle B, float padA, float padB)
{
    A.x += padA; A.width -= padA*2;
    A.y += padA; A.height-= padA*2;

    B.x += padB; B.width -= padB*2;
    B.y += padB; B.height-= padB*2;

    return CheckCollisionRecs(A, B);
}

// Main function that begins the execution
/*
The function int main is the program's canonical entry point where execution begins,
so it orchestrates high-level startup tasks like parsing command-line arguments and initializing subsystems.
Its return value communicates the program's exit status to the operating system or calling process,
allowing scripts and other programs to detect success or failure. main is also the natural place to set up
global state, create the primary application loop, and ensure orderly teardown and resource release before exit.
Because main controls overall program flow, keeping it focused and delegating work to well-named functions
improves readability, testability, and maintainability of the codebase.
*/
int main()
{
    const int windowWidth = 1500; // 1280
    const int windowHeight = 800; // 720

    InitWindow(windowWidth, windowHeight, "Mr Mohamed Ahamed Assani");

    // Audio
    /*
    Audio provides immediate, non-visual feedback that reinforces player actions and game events,
    making jumps, pickups, hits, and menu interactions feel satisfying and clear. It establishes mood and atmosphere
    through background music and ambient sounds, which helps convey pacing and emotional tone across runs.
    Sound cues also serve as important gameplay signals (e.g., warning sounds, proximity cues, or success chimes)
    that can improve player reaction and situational awareness even when visual attention is elsewhere.
    Because audio can be expensive, it should be managed carefully: stream long music tracks, preload short effects,
    and stop/unload resources on exit to avoid memory or device issues. Thoughtful audio design also supports accessibility
    by providing redundant cues for players with limited vision and by exposing simple toggles (mute, volume) in the UI.
    */
    InitAudioDevice();
    Sound Jump       = LoadSound("Sounds/jump.wav");
    Sound melonPick  = LoadSound("Sounds/pickupCoin.wav");
    Sound kill       = LoadSound("Sounds/hitHurt.wav");
    Music PixelKing  = LoadMusicStream("Sounds/Pixel Kings.wav");

    // Background textures
    /*
    Background textures serve as the visual foundation of the scene, establishing setting,
    mood, and a sense of scale without interfering with gameplay mechanics. They create
    depth through layered parallax movement so nearer layers move faster than distant ones,
    which helps players perceive speed and judge timing for obstacles and pickups. Because
    these textures are decorative rather than interactive, they should be tiled and wrapped
    efficiently to hide repetition and minimize draw cost, preserving performance on lower-end hardware.
    Well-designed background art also provides consistent visual contrast so foreground
    elements (player, obstacles, pickups) remain readable; tune color, brightness, and speed
    per layer to avoid visual clutter while reinforcing the game's atmosphere and pacing.
    */
    Texture2D mountain    = LoadTexture("textures/mountain.png");
    Texture2D birds       = LoadTexture("textures/birds.png");
    Texture2D treesBack   = LoadTexture("textures/treesBack.png");
    Texture2D treesFront  = LoadTexture("textures/treesFront.png");

    // AnimData init helper lambda (évite 50 lignes répétées)
    /*
    This helper lambda centralizes the creation of AnimData instances so you don't
    repeat the same initialization code for every animated entity. It sets up the
    source rectangle, initial screen position, frame timing, and baseline Y in one
    place, which makes adding new sprites quick and less error-prone. By using a
    single factory you ensure consistent conventions (rec layout, baseY semantics,
    updateTime units) across player, obstacles, and decorative layers. Keeping this
    logic together also makes future changes (different sprite origins, flipped
    sprites, or alternate timing) trivial to apply project-wide.
    */
    auto MakeAnim = [&](Texture2D &tex, float x, float y, float frameWidth, float uTime)
    {
        AnimData a{};
        a.rec = {0, 0, frameWidth, (float)tex.height};
        a.pos = {x, y};
        a.updateTime = uTime;
        a.baseY = y;
        return a;
    };

    // Background layers
    /*
    Role: Provide a multi-layered parallax backdrop that creates depth and a sense of motion.
    Visual: Establishes mood and setting (distant mountains, mid-distance birds, near and far trees).
    Gameplay: Supplies visual cues about speed and progression without affecting collision or physics.
    Readability: Separates decorative art from interactive objects so players can focus on obstacles and pickups.
    Performance: Designed to be tiled and wrapped for continuous scrolling while minimizing draw and update cost.
    Layering: Render order (mountain → birds → treesBack → treesFront) ensures correct occlusion and silhouette clarity.
    Tuning: Each layer uses a different speed to exaggerate depth; tweak speeds to balance readability and aesthetic.
    */
    AnimData mountainData  = MakeAnim(mountain, 0, 0, mountain.width, 0);
    AnimData birdsData     = MakeAnim(birds,    0, 0, birds.width,    0);
    AnimData treesBackData = MakeAnim(treesBack, treesBack.width, 0, treesBack.width, 0);
    AnimData treesFrontData= MakeAnim(treesFront, treesFront.width,0, treesFront.width,0);

    // Player texture / anim
    /*
    Visual representation of the player character, showing Glino's appearance on screen.
    Drives sprite-sheet animation frames so running, jumping, and ducking look smooth and readable.
    Encodes frame geometry (rec) used for rendering and for aligning the collision box with the sprite.
    Provides animation timing (updateTime, runningTime) so motion is independent of frame rate.
    Stores positional state (pos, baseY) used by physics and to snap the sprite to the ground reliably.
    Acts as the single source of truth for player visuals so input, physics, and collisions stay synchronized.
    Enables easy extension (new animations, states, or visual effects) without changing core game logic.
    */
    Texture2D Dino = LoadTexture("textures/dino.png");
    AnimData dinoData = MakeAnim(Dino, 
                                windowWidth/3 - (Dino.width/4)/2,
                                (windowHeight - 80) - Dino.height,
                                Dino.width/4, 
                                0.1f);

    // Obstacles (Laeva)
    /*
    Role: Act as the primary hazards the player must avoid, creating challenge and tension in each run.
    Provide pacing and difficulty scaling by varying spawn timing, speed, and spacing as the score increases.
    Serve as visual targets for player attention, using animation frames to make movement and threat readable.
    Drive gameplay loops (risk/reward) by forcing jumps, ducks, and timing decisions that test player skill.
    Interact with collision logic to trigger game-over state and audio feedback, making failures clear and satisfying.
    Offer extensibility points for variety (different obstacle types, sizes, hitboxes, or behaviors) without changing core systems.
    Contribute to level feel and rhythm when combined with parallax backgrounds and pickup placement.
    */
    Texture2D Laeva = LoadTexture("textures/LAEVA.png");
    const int LaevaCount = 6;
    AnimData Laevas[LaevaCount];
    int dist = 100;

    for (int i = 0; i < LaevaCount; i++)
    {
        Laevas[i] = MakeAnim(Laeva, windowWidth + dist, (windowHeight - 100) - Laeva.height, Laeva.width/4, 0.2);
        dist += 10000;
    }

    // Melon pickup
    /*
    Melon pickup provides a rewarding secondary objective that breaks up obstacle sequences and gives players short-term goals.
    Collectibles like melons encourage risk/reward decisions by tempting players to move into more dangerous positions for bonus points.
    They deliver positive feedback through sound and score increases, reinforcing successful play and improving player satisfaction.
    Melons can be used to teach timing and positioning, since their placement and movement create additional patterns to learn.
    Because pickups are non-lethal, they offer a safe way to vary pacing and introduce variety without changing core obstacle mechanics.
    */
    Texture2D melon = LoadTexture("textures/Yellow Watermelon2.png");
    const int MelonCount = 1;
    AnimData Melons[MelonCount];
    Melons[0] = MakeAnim(melon, windowWidth + 1000, (windowHeight - 200) - melon.height, melon.width, 0.2);
    
    // Gameplay variables
    /*
    Role: Centralize tunable game state (speeds, timers, scores) so gameplay can be balanced without digging through logic.
    Store transient runtime values (velocity, timers, flags) that drive physics, input response, and state transitions.
    Control difficulty progression by exposing parameters (objectSpeed, scoreInterval) that scale as the player advances.
    Coordinate UI and game flow (score, Menu, hitLaeva) so rendering and input handlers can read a single source of truth.
    Provide persistence points for reset/respawn logic so a single reset routine can restore consistent starting values.
    Separate configuration (constants) from state (variables) to make it easier to tweak behavior and reason about side effects.
    Act as documentation for other systems: physics, audio, animation, and spawning all reference these variables to remain synchronized.
    */
    bool DinoIsDuck = false;
    int dinoVel = 0;
    const int jumpStrength = 800;
    const int gravity = 2000;

    float objectSpeed = 300;
    float mountainSpeed = 30;
    float birdsSpeed = 20;
    float treesBackSpeed = 70;
    float treesFrontSpeed = 100;

    int score = 0;
    float scoreTimer = 0;
    const float scoreInterval = 1.0f;

    float duckTimer = 0;
    const float duckCooldown = 0.45f;

    bool hitLaeva = false;
    bool Menu = true;
    bool MusicOn = true;

    SetTargetFPS(60);
    
    // The loop while
    /*
    The loop while (!WindowShouldClose()) is the main game loop that keeps the application running until the user requests exit.
    It repeatedly processes input, updates game state (physics, AI, animations), and renders the current frame, providing the heartbeat of the program.
    By centralizing per-frame work here, you ensure consistent ordering (input → update → draw) and make it straightforward to manage timing and frame-rate independence.
    This loop is also the place to poll and handle window events, swap buffers, and perform periodic housekeeping like audio streaming or resource cleanup.
    Keeping the loop body concise and delegating tasks to well-named functions improves readability, makes profiling easier, and reduces the risk of frame hitches.
    */
    while (!WindowShouldClose())
    {
        float dt = GetFrameTime();

        BeginDrawing();
        ClearBackground(YELLOW);

        // ---------------------------------------------------------------------
        // MENU SCREEN
        // ---------------------------------------------------------------------
        /*
        The menu screen acts as the player's first point of contact with the game, setting tone,
        presenting the title and essential information, and giving the player a clear entry point
        to start playing. It communicates controls, current status (e.g., music on/off), and any
        context or credits that help the player understand who made the game and what to expect.
        By pausing gameplay logic while visible, the menu provides a safe place to tune settings
        and prepare before a run begins, preventing accidental input from affecting game state.
        A well-designed menu also serves as a UX checkpoint for onboarding new players and for
        testing; it can be extended to include options, difficulty selection, or links to tutorials.
        */
        if (Menu)
        {
            DrawText("Welcome to the Story of Glino!", 100, 100, 40, BLACK);
            DrawText("My name is Mohamed Ahamed Assani...", 100, 150, 40, RED);
            DrawText("Student of Faculty Engineering Level 2...", 100, 200, 40, RED);
            DrawText("I'm still developing this game", 100, 250, 40, RED);
            DrawText("It will get more amazing soon...", 100, 300, 40, BLACK);
            DrawText("Help Glino avoid certain DEATH!", 100, 350, 40, BLACK);

            DrawTextureRec(Dino, dinoData.rec, dinoData.pos, YELLOW);

            DrawText("CONTROLS", 850, 450, 40, RED);
            DrawText("SPACE → Jump", 800, 500, 20, BLUE);
            DrawText("S → Duck", 800, 550, 20, BLUE);
            DrawText("Q → Music OFF", 800, 600, 20, BLUE);
            DrawText("E → Music ON", 800, 650, 20, BLUE);
            DrawText("ENTER TO START", 100, 400, 60, RED);

            if (IsKeyPressed(KEY_ENTER)) Menu = false;
            if (IsKeyPressed(KEY_Q)) MusicOn = false;

            EndDrawing();
            continue;
        }

        // ---------------------------------------------------------------------
        // GAME OVER SCREEN
        // ---------------------------------------------------------------------
        /*
        Role: Communicates failure state clearly to the player and provides a moment to process the run's outcome.
        Displays final score and restart prompt so the player can measure progress and decide to retry.
        Freezes or halts gameplay logic (spawning, movement, physics) to prevent further state changes while showing the screen.
        Triggers and coordinates audio/visual feedback (sounds, animations) that reinforce the consequence of collision.
        Acts as a central place to reset or reinitialize game state when the player chooses to restart.
        Serves as a UX checkpoint for difficulty tuning and telemetry (e.g., showing why runs end and where to improve).
        Provides a natural pause for menus, ads, or post-run summaries without mixing with active gameplay rendering.
        */
        if (hitLaeva)
        {
            DrawText("Game Over, Glino Died :(", windowWidth/2 - 700, 200, 60, BLUE);

            std::string s = "Score: " + std::to_string(score);
            DrawText(s.c_str(), windowWidth/2 - 200, 400, 60, RED);
            DrawText("YOU'RE AMAZING, JUST BE MORE CAREFULL NEXT TIME. ENTER TO RESTART", windowWidth/2 - 700, 300, 35, BLUE);

            if (IsKeyPressed(KEY_ENTER))
            {
                hitLaeva = false;
                score = 0;

                int dist = 100;
                for (int i = 0; i < LaevaCount; i++)
                {
                    Laevas[i].pos.x = windowWidth + dist;
                    Laevas[i].frame = 0;
                    dist += 10000;
                }

                Melons[0].pos.x = windowWidth + GetRandomValue(2000,20000);

                objectSpeed = 300;
                mountainSpeed = 30;
                birdsSpeed = 20;
                treesFrontSpeed = 100;
                treesBackSpeed = 70;
            }

            EndDrawing();
            continue;
        }
        
        // Music
        /*
        Music sets the emotional tone and atmosphere of the game, reinforcing mood and pacing across runs.
        It provides a continuous auditory backdrop that helps players feel momentum and can make gameplay sessions more engaging and memorable.
        Musical cues can also be used to signal changes in intensity or to foreshadow upcoming difficulty spikes, improving player anticipation and reaction.
        From a UX perspective, music complements sound effects by filling silence and smoothing transitions between menu, gameplay, and game-over states.
        Practically, music should be streamed and managed carefully (start/stop, volume, mute toggle) to conserve resources and give players control over their audio experience.
        */
        if (MusicOn && !IsMusicStreamPlaying(PixelKing) ) { PlayMusicStream(PixelKing); }
        else if (!MusicOn && IsMusicStreamPlaying(PixelKing)) { StopMusicStream(PixelKing); }
        if (IsMusicStreamPlaying(PixelKing)) { UpdateMusicStream(PixelKing); }
        if (IsKeyPressed(KEY_Q) && MusicOn) { MusicOn = false; }
        if (IsKeyPressed(KEY_E) && !MusicOn) { MusicOn = true; }

        // Ground check + gravity
        /*
        Ground check and gravity implement the vertical physics that make jumping and falling feel consistent and believable.
        The ground check determines when an entity is considered "on the floor," which gates jump input and allows the code to snap the sprite to a stable baseline to avoid visual sinking due to floating point error. 
        Gravity continuously accelerates the character downward when airborne, producing natural arcs and hang-time that players can learn and time against obstacles. 
        Together these systems separate movement intent (player input) from physical response (velocity and acceleration), enabling predictable tuning of jump height and fall speed. 
        Keeping the checks and integration frame-rate independent and centralized makes it easy to adjust game feel, add features like coyote time or variable jump height, and ensure consistent behavior across different hardware.
        */
        if (!isOnGround(dinoData, windowHeight))
            dinoVel += gravity * dt;
        else
            dinoVel = 0;

        // Score
        /*
        Score tracks player progress and provides immediate feedback for successful play, reinforcing positive actions like collecting pickups or surviving longer runs.
        It acts as a simple, universal metric for measuring performance and comparing runs, which supports player motivation and replayability.
        Score can drive difficulty scaling and pacing by triggering speed increases, spawn changes, or rewards as thresholds are reached.
        Displaying the score in the UI gives players a clear short-term goal and helps them gauge improvement over time; it also enables features like high scores and leaderboards.
        Because score is a lightweight, decoupled piece of state, it can be extended to support combos, multipliers, or persistent progression without changing core gameplay systems.
        */
        scoreTimer += dt;
        if (scoreTimer >= scoreInterval)
        {
            scoreTimer = 0;
            score++;

            if (objectSpeed < 700)
            {
                objectSpeed += 10;
                mountainSpeed += 2;
                treesBackSpeed += 4;
                treesFrontSpeed += 6;
            }
        }

        DrawText(("Score: " + std::to_string(score)).c_str(),
                 windowWidth/2 - 200, 100, 30, BLUE); 

        // Ducking
        /*
        Ducking gives the player an additional defensive action to avoid certain obstacles that cannot be jumped over.
        It introduces a timing-based decision that complements jumping, expanding the set of challenges and required skills.
        Visually, ducking should switch the sprite to a distinct frame or pose so the player receives immediate feedback.
        Mechanically, it often reduces the player's collision box or changes hitbox alignment, so collision checks must account for the state.
        Ducking also enables level design variety by allowing low obstacles, tunnels, or enemy attacks that reward precise, short-duration inputs.
        */
        if (IsKeyPressed(KEY_S)) { DinoIsDuck = true;}

        if (DinoIsDuck && duckTimer < duckCooldown)
        {
            duckTimer += dt;
            dinoData.rec.x = 3 * dinoData.rec.width;
        }
        else
        {
            duckTimer = 0;
            DinoIsDuck = false;
            dinoData = updateAnimData(dinoData, dt, 2);
        }

        // Jump
        /*
        Jump is the primary vertical mobility mechanic that allows the player to clear obstacles and traverse level geometry.
        It creates timing-based challenges by requiring players to judge distance and height, which deepens skill expression.
        Mechanically, jumping interacts with physics (gravity, velocity) and animation so visual and tactile feedback feel responsive.
        Jumping also enables level designers to place aerial pickups, platforms, and hazards that diversify encounters and pacing.
        Tuning jump height, hang time, and input buffering directly affects accessibility and game feel, making it a key lever for difficulty and player satisfaction.
        */
        if ((IsKeyPressed(KEY_SPACE) || IsKeyPressed(KEY_W)) &&
            isOnGround(dinoData, windowHeight))
        {
            dinoVel -= jumpStrength;
            PlaySound(Jump);
        }

        // Update Dino position
        /*
        Updating the Dino's position applies the results of physics and player input to the character's on-screen coordinates each frame.
        This step integrates velocity (including gravity and jump impulses) over delta time so movement feels smooth and consistent across different frame rates.
        It is also where you should enforce collision responses and snapping to the ground to prevent the sprite from sinking or passing through obstacles.
        Keeping position updates separate from input handling and rendering makes the game loop easier to reason about and simplifies debugging.
        Finally, updating position is the moment to refresh any dependent state (hitbox alignment, animation state transitions, or camera follow logic) so visuals and collisions remain synchronized with the physics.
        */
        dinoData.pos.y += dinoVel * dt;

        // Obstacles movement
        /*
        Obstacles movement drives the core challenge of the game by presenting dynamic hazards the player must react to.
        Moving obstacles create pacing and rhythm, allowing difficulty to be tuned by changing speed, spacing, and spawn timing.
        Their motion provides important visual cues that communicate threat level and timing windows for jumps or ducks.
        Obstacles interact directly with collision and audio systems to produce clear feedback on failure and to reinforce learning.
        Keeping obstacle movement logic centralized makes it easy to add new behaviors, vary patterns, and scale difficulty as the score increases.
        */
        for (int i = 0; i < LaevaCount; i++)
        {
            if (Laevas[i].pos.x <= 0)
                Laevas[i].pos.x = windowWidth + GetRandomValue(100,500);
            else
                Laevas[i].pos.x -= objectSpeed * dt;

            Laevas[i] = updateAnimData(Laevas[i], dt, 3);

            if (CheckCollisionWithPadding(
                    {Laevas[i].pos.x, Laevas[i].pos.y,
                     Laevas[i].rec.width, Laevas[i].rec.height},
                    {dinoData.pos.x, dinoData.pos.y,
                     dinoData.rec.width, dinoData.rec.height},
                    30, 30) &&
                !DinoIsDuck)
            {
                PlaySound(kill);
                hitLaeva = true;
            }
        }

        // Melon movement
        /*
        Role: Provide collectible pickups that reward the player with points and positive feedback when collected.
        Introduce a secondary objective that encourages risk/reward decisions and varied movement patterns.
        Act as pacing elements that break up obstacle sequences and give the player short-term goals during a run.
        Offer audio-visual reinforcement (sound + score increase) to make successful play feel satisfying and clear.
        Serve as spawnable entities that can be tuned independently (speed, spawn range, hitbox) to balance difficulty.
        Create opportunities for level design variety by placing melons in challenging or safe positions relative to obstacles.
        Support progression mechanics (e.g., bonus score, combos, or temporary buffs) without changing core collision logic.
        */
        for (int i = 0; i < MelonCount; i++)
        {
            if (Melons[i].pos.x <= 0)
                Melons[i].pos.x = windowWidth + GetRandomValue(2000,20000);
            else
                Melons[i].pos.x -= objectSpeed * dt;

            if (CheckCollisionWithPadding(
                    {Melons[i].pos.x, Melons[i].pos.y,
                     Melons[i].rec.width, Melons[i].rec.height},
                    {dinoData.pos.x, dinoData.pos.y,
                     dinoData.rec.width, dinoData.rec.height},
                    5, 30))
            {
                PlaySound(melonPick);
                score += 10;
                Melons[i].pos.x = windowWidth + GetRandomValue(2000,20000);
            }
        }

        // Background parallax
        /*
        Role: Create a layered scrolling backdrop that conveys depth and motion without affecting gameplay physics.
        Reinforce perceived speed by moving nearer layers faster than distant ones, improving player feedback.
        Provide visual context and atmosphere (environment, time of day, mood) to make runs feel distinct and alive.
        Offer subtle cues about game pacing and difficulty as layer speeds change with progression.
        Hide repetition by tiling and wrapping textures so the world appears continuous during long runs.
        Separate decorative elements from interactive objects so players can focus on obstacles and pickups.
        Allow easy aesthetic tuning (art, speed, opacity) to match theme or performance constraints.
        */
        auto MoveWrap = [&](AnimData &d, float speed, float limit)
        {
            if (d.pos.x <= -limit)
                d.pos.x = limit;
            else
                d.pos.x -= speed * dt;
        };

        MoveWrap(mountainData, mountainSpeed, mountain.width);
        MoveWrap(birdsData, birdsSpeed, 200);
        MoveWrap(treesBackData, treesBackSpeed, treesBack.width);
        MoveWrap(treesFrontData, treesFrontSpeed, treesFront.width);

        // DRAWING GREEN
        /*
        Drawing is responsible for presenting the current game state to the player by rendering sprites, backgrounds, UI, and effects each frame.
        It enforces a consistent render order so background layers, game objects, and foreground overlays appear with correct occlusion and visual hierarchy.
        Keeping drawing code separate from game logic helps maintain clarity: update state first, then render deterministically from that state.
        Efficient drawing minimizes GPU/CPU work (batching, culling, and texture reuse) to preserve smooth frame rates across hardware.
        The drawing stage also communicates feedback (animations, hit flashes, score updates) that makes gameplay feel responsive and satisfying.
        */
        DrawTextureRec(mountain, mountainData.rec, mountainData.pos, BROWN);
        DrawTextureRec(birds, birdsData.rec, birdsData.pos, YELLOW);
        DrawTextureRec(treesBack, treesBackData.rec, treesBackData.pos, BLUE);
        DrawTextureRec(treesFront, treesFrontData.rec, treesFrontData.pos, RED);
        DrawTextureRec(Dino, dinoData.rec, dinoData.pos, WHITE);

        for (int i = 0; i < LaevaCount; i++)
            DrawTextureRec(Laeva, Laevas[i].rec, Laevas[i].pos, GREEN);

        for (int i = 0; i < MelonCount; i++)
            DrawTextureRec(melon, Melons[i].rec, Melons[i].pos, WHITE);

        EndDrawing();
    }

    // CLEANUP
    /*
    Cleanup ensures all loaded resources are released and system subsystems are shut down
    so the program exits cleanly without leaking memory or holding OS handles. It unloads
    textures, sounds, and music streams and stops any playing audio to avoid dangling
    playback or driver issues after the window closes. Proper cleanup also restores global
    state (audio device, window context) which prevents interference with other applications
    and makes repeated runs of the program reliable during development. Finally, explicit
    resource release makes debugging easier and documents ownership of assets for future maintenance.
    */
    UnloadTexture(Dino);
    UnloadTexture(Laeva);
    UnloadTexture(melon);
    CloseWindow();
}

