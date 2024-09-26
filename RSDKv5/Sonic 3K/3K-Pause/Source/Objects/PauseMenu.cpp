#include "3K-Pause.hpp"

using namespace RSDK;

namespace GameLogic
{

// -------------------
// Object Registration
// -------------------

MOD_REGISTER_OBJECT(PauseMenu);

// -------------
// Entity Events
// -------------

void PauseMenu::Update()
{
    state.Run(this);
    selectionAnimator.Process();

    ControllerState *controller = &controllerInfo[Input::CONT_P1];
    AnalogState *stick          = &analogStickInfoL[Input::CONT_P1];

    up = controller->keyUp.press;
    up |= stick->keyUp.press;

    down = controller->keyDown.press;
    down |= stick->keyDown.press;

    start = controller->keyStart.press;
    start |= controller->keyA.press;
    start |= unknownInfo->pausePress;
}

void PauseMenu::LateUpdate()
{
    auto pState = GameObject::Get<Player>(triggerPlayer)->state.state;

    if (state.Matches(StateMachine_None)) {
        if (1 == 0 /*pState.Matches(&Player::State_Death) || pState.Matches(&Player::State_Drown)*/) {
            Destroy();
        }
        else {
            visible   = true;
            drawGroup = DRAWGROUP_COUNT - 1;
            state.Set(&PauseMenu::State_StartPause);
        }
    }
}

void PauseMenu::StaticUpdate() {}

int32 temp0;

void PauseMenu::Draw()
{
    // 5 = Category Special

    if (sceneInfo->activeCategory == 5)
        paletteBank[3].SetActivePalette(0, 255);

    stateDraw.Run(this);

    if (sceneInfo->activeCategory == 5)
        paletteBank[0].SetActivePalette(0, 255);
}

void PauseMenu::Create(void *data)
{
    animator.SetAnimation(modSVars->aniFrames, 0, true, 0);

    switch (GET_CHARACTER_ID(1)) {
        default:
        case ID_NONE:
        case ID_SONIC: selectionAnimator.SetAnimation(modSVars->aniFrames, 1, true, 0); break;
        case ID_TAILS: selectionAnimator.SetAnimation(modSVars->aniFrames, 2, true, 0); break;
        case ID_KNUCKLES:
            selectionAnimator.SetAnimation(modSVars->aniFrames, 3, true, 0);
            break;
            // case ID_AMY: selectionAnimator.SetAnimation(modSVars->aniFrames, 4, true, 0); break;
    }

    backgroundAlpha = 74;

    if (!sceneInfo->inEditor) {
        active = ACTIVE_ALWAYS;

        if (data == INT_TO_VOID(true)) {
            visible   = true;
            drawGroup = DRAWGROUP_COUNT - 1;
        }
        else {
            state.Set(StateMachine_None);
            stateDraw.Set(StateMachine_None);
        }
    }
}

void PauseMenu::StageLoad()
{
    sVars->active = ACTIVE_ALWAYS;

    modSVars->aniFrames.Load("3K_Global/Pause.bin", SCOPE_STAGE);
    modSVars->sfxBleep.Get("Global/MenuBleep.wav");
    modSVars->sfxAccept.Get("Global/MenuAccept.wav");
    modSVars->sfxWoosh.Get("Global/MenuWoosh.wav");

    for (int32 i = 0; i < CHANNEL_COUNT; ++i) modSVars->activeChannels[i] = false;
}

// Extra Entity Functions
void PauseMenu::DrawPauseRect(int32 y)
{
    // 'PAUSE' rect left
    Graphics::DrawRect(position.x + 128, NULL, 128, screenInfo->size.y, 0x000000, 0xFF, INK_NONE, true);

    // 'PAUSE' text sprite
    animator.frameID = 4;
    drawPos.x        = TO_FIXED(pauseBarPos.x - screenInfo->size.x);
    drawPos.y        = TO_FIXED(y);
    animator.DrawSprite(&drawPos, true);

    // 'PAUSE' rect right
    Graphics::DrawRect(pauseBarPos.x - screenInfo->size.x + 106, y, screenInfo->size.x, 13, 0x000000, 0xFF, INK_NONE, true);
}

void PauseMenu::DrawSpikes()
{
    for (int32 i = 0; i < 8; i++) {
        animator.DrawSprite(&drawPos, true);
        drawPos.y += TO_FIXED(32);
    }
}

void PauseMenu::PauseSound()
{
    for (int32 i = 0; i < CHANNEL_COUNT; ++i) {
        if (channels[i].IsActive()) {
            channels[i].Pause();
            modSVars->activeChannels[i] = true;
        }
    }
}

void PauseMenu::ResumeSound()
{
    for (int32 i = 0; i < CHANNEL_COUNT; ++i) {
        if (modSVars->activeChannels[i]) {
            channels[i].Resume();
            modSVars->activeChannels[i] = false;
        }
    }
}

void PauseMenu::StopSound()
{
    for (int32 i = 0; i < CHANNEL_COUNT; ++i) {
        if (modSVars->activeChannels[i]) {
            channels[i].Stop();
            modSVars->activeChannels[i] = false;
        }
    }
}

void PauseMenu::Action_Accept()
{
    modSVars->sfxAccept.Play();

    state.Set(&PauseMenu::State_Confirmed);
    stateDraw.Set(&PauseMenu::Draw_Confirmed);

    alpha = 0xF8;
    timer = 0;
}

// Object States
void PauseMenu::State_StartPause()
{
    SET_CURRENT_STATE();

    auto player = GameObject::Get<Player>(SLOT_PLAYER1);

    PauseSound();
    Stage::SetEngineState(ENGINESTATE_FROZEN);
    modSVars->sfxWoosh.Play();

    position.x = screenInfo->size.x;
    spritePosX = screenInfo->size.x + 6;
    timer      = 0;
    alpha      = 0;

    // TODO: Add language switch for this(?)
    timerThreshold = 12;

    // 2 = StageCategoryCompetition
    // 4 = StageCategoryBonus
    // 5 = StageCategorySpecial

    bool32 blockedCategories = sceneInfo->activeCategory == 2 || sceneInfo->activeCategory == 5 || sceneInfo->activeCategory == 4;
    if (player->lives <= 1 || blockedCategories)
        disableRestart = true;
    else
        disableRestart = false;

    if (config.hasTouchControls)
        selectedButton = -1;

    state.Set(&PauseMenu::State_SlideIn);
    stateDraw.Set(&PauseMenu::Draw_Init);
}

void PauseMenu::State_SlideIn()
{
    SET_CURRENT_STATE();

    pauseBarPos.x = (timer * screenInfo->size.x) / 12;
    pauseBarPos.y = (timer * screenInfo->size.y) / 12;
    position.x    = screenInfo->size.x - (timer << 7) / 12;

    if (timer++ >= timerThreshold) {
        timer      = 0;
        spritePosX = (position.x + 48);
        state.Set(&PauseMenu::State_SpritesFadeIn);
    }
}

void PauseMenu::State_SpritesFadeIn()
{
    SET_CURRENT_STATE();

    if (timer <= 0xFF) {
        timer += 16;
        alpha = (timer < 0xFF) ? timer : 0xFF;
    }
    else {
        timer = 0;
        alpha = 0xFF;
        state.Set(&PauseMenu::State_Controls);
        stateDraw.Set(&PauseMenu::Draw_Controls);
    }
}

void PauseMenu::State_Controls()
{
    SET_CURRENT_STATE();

    if (!config.hasTouchControls) {
        if (up || down) {
            timer      = 0;
            spritePosX = screenInfo->size.x;
            modSVars->sfxBleep.Play();
        }

        if (up) {
            // decrease selection and wrap around
            if (--selectedButton < 0)
                selectedButton = 3;

            if (disableRestart && selectedButton == 1)
                selectedButton--;
        }

        if (down) {
            // increase selection and wrap around
            if (++selectedButton > 3)
                selectedButton = 0;

            if (disableRestart && selectedButton == 1)
                selectedButton++;
        }

        if (start) {
            Action_Accept();
        }
    }
    else {
        if (Touch::CheckRect(spritePosX, 32, screenInfo->size.x, 64, nullptr, nullptr) >= 0) {
            selectedButton = 0;
        }
        else {
            if (CheckAnyTouch() < 0) {
                if (selectedButton == 0) {
                    Action_Accept();
                }
                else {
                    if (selectedButton == 0)
                        selectedButton = -1;
                }
            }
        }

        // Restart
        if (!disableRestart) {
            if (Touch::CheckRect(spritePosX, 80, screenInfo->size.x, 112, nullptr, nullptr) >= 0) {
                selectedButton = 1;
            }
            else {
                if (CheckAnyTouch() < 0) {
                    if (selectedButton == 1) {
                        Action_Accept();
                    }
                    else {
                        if (selectedButton == 1)
                            selectedButton = -1;
                    }
                }
            }
        }

        // Options
        if (Touch::CheckRect(spritePosX, 128, screenInfo->size.x, 160, nullptr, nullptr) >= 0) {
            selectedButton = 2;
        }
        else {
            if (CheckAnyTouch() < 0) {
                if (selectedButton == 2) {
                    Action_Accept();
                }
            }
            else {
                if (selectedButton == 2)
                    selectedButton = -1;
            }
        }

        // Exit
        if (Touch::CheckRect(spritePosX, 176, screenInfo->size.x, 208, nullptr, nullptr) >= 0) {
            selectedButton = 3;
        }
        else {
            if (CheckAnyTouch() < 0) {
                if (selectedButton == 3) {
                    Action_Accept();
                }
            }
            else {
                if (selectedButton == 3)
                    selectedButton = -1;
            }
        }

        if (up) {
            selectedButton = 3;
        }

        if (down) {
            selectedButton = 0;
        }

        if (timer < 60)
            timer++;
    }
}

void PauseMenu::State_Confirmed()
{
    SET_CURRENT_STATE();

    spritePosX += 4;

    timer = (timer + 1) & 3;
    (alpha > 0) ? alpha -= 12 : alpha = 0;

    if (!alpha) {
        // Check for Resume (and options for now)
        if (selectedButton == -1 || selectedButton == 0 || selectedButton == 2) {
            spritePosX    = position.x + 48;
            pauseBarPos.y = 202;

            timer = 0;
            alpha = backgroundAlpha; // Used for the BG, return to the initial opacity and then fade out
            state.Set(&PauseMenu::State_ResumeGame);
            stateDraw.Set(&PauseMenu::Draw_ResumeGame);
            Stage::SetEngineState(ENGINESTATE_REGULAR);
        }
        else {
            // globals->recallEntities = false;
            // globals->initCoolBonus  = false;
            /*
            if (StarPost) {
                StarPost->postIDs[0] = 0;
                StarPost->postIDs[1] = 0;
                StarPost->postIDs[2] = 0;
                StarPost->postIDs[3] = 0;
            }
            */
            // globals->specialRingID = 0;

            // Music::Stop();

            spritePosX = position.x + 48;
            state.Set(&PauseMenu::State_ExitLevel);
            stateDraw.Set(&PauseMenu::Draw_ExitLevel);
        }
    }
}

void PauseMenu::State_ResumeGame()
{
    SET_CURRENT_STATE();

    if (alpha > 0) {
        alpha -= 8;
        position.x += 16;
        spritePosX += 16;
        pauseBarPos.y += 16;
    }
    else {
        ResumeSound();
        Destroy();
    }
}

void PauseMenu::State_ExitLevel()
{
    SET_CURRENT_STATE();

    if (position.x > -64) {
        position.x -= 16;
        spritePosX += 16;
    }
    else {
        switch (selectedButton) {
            // Continue - Do nothing
            case 0: break;

            // Restart
            case 1: Stage::LoadScene(); break;

            // Options
            case 2:
                // TimeAttack.Round = -1;
                // Stage::SetEngineState(ENGINESTATE_DEVMENU);
                break;

            // Exit
            case 3:
                // 2 = StageCategoryCompetition
                if (sceneInfo->activeCategory == 2)
                    Stage::SetScene("Presentation & Menus", "Menu Comp");
                else
                    Stage::SetScene("Presentation & Menus", "Title Screen");

                Stage::LoadScene();
                break;

            default: break;
        }
    }
}

// Draw States
void PauseMenu::Draw_Init()
{
    SET_CURRENT_STATE();

    inkEffect = INK_NONE;

    if (pauseBarHeight < SCREEN_YSIZE && pauseBarPos.y > pauseBarHeight) {
        pauseBarHeight = pauseBarPos.y;
    }

    Draw_Controls();
}

void PauseMenu::Draw_Controls()
{
    SET_CURRENT_STATE();

    inkEffect = INK_NONE;

    // Background fade
    Graphics::DrawRect(NULL, NULL, screenInfo->size.x, pauseBarHeight, 0x000000, backgroundAlpha, INK_ALPHA, true);

    // 'PAUSE' bar
    DrawPauseRect(202);

    // Spikes
    animator.frameID = 3;
    drawPos.x        = TO_FIXED(position.x);
    drawPos.y        = 0;
    DrawSpikes();

    inkEffect = INK_ALPHA; // eh

    temp0 = position.x + 48;
    if (spritePosX > temp0) {
        spritePosX += temp0 - spritePosX >> 3;
    }

    // Draw 'CONTINUE' button
    animator.frameID = 6;
    drawPos.x        = TO_FIXED(temp0);
    drawPos.y        = TO_FIXED(48);
    animator.DrawSprite(&drawPos, true);

    if (selectedButton == 0) {
        // Selection Line
        animator.frameID = 5;
        drawPos.x        = TO_FIXED(spritePosX);
        animator.DrawSprite(&drawPos, true);

        // Selection Player
        drawPos.x = TO_FIXED(position.x + 48);
        selectionAnimator.DrawSprite(&drawPos, true);
    }

    // Draw 'RESTART' button
    animator.frameID = 7;
    drawPos.x        = TO_FIXED(temp0);
    drawPos.y += TO_FIXED(48);

    if (!disableRestart) {
        animator.DrawSprite(&drawPos, true);
    }
    else {
        alpha >>= 1;
        animator.DrawSprite(&drawPos, true);
        alpha <<= 1;
    }

    if (selectedButton == 1) {
        // Selection Line
        animator.frameID = 5;
        drawPos.x        = TO_FIXED(spritePosX);
        animator.DrawSprite(&drawPos, true);

        // Selection Player
        drawPos.x = TO_FIXED(position.x + 48);
        selectionAnimator.DrawSprite(&drawPos, true);
    }

    // Draw 'OPTIONS' button
    animator.frameID = 8;
    drawPos.y += TO_FIXED(48);
    animator.DrawSprite(&drawPos, true);

    if (selectedButton == 2) {
        // Selection Line
        animator.frameID = 5;
        drawPos.x        = TO_FIXED(spritePosX);
        animator.DrawSprite(&drawPos, true);

        // Selection Player
        drawPos.x = TO_FIXED(position.x + 48);
        selectionAnimator.DrawSprite(&drawPos, true);
    }

    // Draw 'EXIT' button
    animator.frameID = 9;
    drawPos.y += TO_FIXED(48);
    animator.DrawSprite(&drawPos, true);

    if (selectedButton == 3) {
        // Selection Line
        animator.frameID = 5;
        drawPos.x        = TO_FIXED(spritePosX);
        animator.DrawSprite(&drawPos, true);

        // Selection Player
        drawPos.x = TO_FIXED(position.x + 48);
        selectionAnimator.DrawSprite(&drawPos, true);
    }
}

void PauseMenu::Draw_Confirmed()
{
    SET_CURRENT_STATE();

    inkEffect = INK_NONE;

    // Background fade
    Graphics::DrawRect(NULL, NULL, screenInfo->size.x, pauseBarHeight, 0x000000, backgroundAlpha, INK_ALPHA, true);

    // 'PAUSE' bar
    DrawPauseRect(202);

    animator.frameID = 3;
    drawPos.x        = TO_FIXED(position.x);
    drawPos.y        = 0;
    DrawSpikes();

    // Continue
    drawPos.y = TO_FIXED(48);
    if (selectedButton != 0) {
        inkEffect        = INK_ALPHA;
        animator.frameID = 6;
        drawPos.x        = TO_FIXED(position.x + 48);
        animator.DrawSprite(&drawPos, true);
    }
    // Continue (Selected)
    else {
        // Continue flash
        if (timer < 2) {
            inkEffect        = INK_NONE;
            animator.frameID = 6;
            drawPos.x        = TO_FIXED(position.x + 48);
            animator.DrawSprite(&drawPos, true);
        }

        // Selection Line
        animator.frameID = 5;
        drawPos.x        = TO_FIXED(spritePosX);
        animator.DrawSprite(&drawPos, true);

        // Selection Player
        drawPos.x = TO_FIXED(position.x + 48);
        selectionAnimator.DrawSprite(&drawPos, true);
    }

    // Restart
    drawPos.y += TO_FIXED(48);
    if (selectedButton != 1) {
        if (!disableRestart) {
            inkEffect        = INK_ALPHA;
            animator.frameID = 7;
            drawPos.x        = TO_FIXED(position.x + 48);
            animator.DrawSprite(&drawPos, true);
        }
        else {
            inkEffect = INK_ALPHA;
            drawPos.x = TO_FIXED(position.x + 48);

            alpha >>= 1;
            animator.frameID = 7;
            animator.DrawSprite(&drawPos, true);
            alpha <<= 1;
        }
    }
    // Restart (Selected)
    else {
        // Restart flash
        if (timer < 2) {
            inkEffect        = INK_NONE;
            drawPos.x        = TO_FIXED(position.x + 48);
            animator.frameID = 7;
            animator.DrawSprite(&drawPos, true);
        }

        // Selection Line
        inkEffect        = INK_NONE;
        drawPos.x        = TO_FIXED(spritePosX);
        animator.frameID = 5;
        animator.DrawSprite(&drawPos, true);

        // Selection Player
        drawPos.x = TO_FIXED(position.x + 48);
        selectionAnimator.DrawSprite(&drawPos, true);
    }

    // Options
    drawPos.y += TO_FIXED(48);
    if (selectedButton != 2) {
        inkEffect        = INK_ALPHA;
        animator.frameID = 8;
        drawPos.x        = TO_FIXED(position.x + 48);
        animator.DrawSprite(&drawPos, true);
    }
    // Options (Selected)
    else {
        // Options flash
        if (timer < 2) {
            inkEffect        = INK_NONE;
            animator.frameID = 8;
            drawPos.x        = TO_FIXED(position.x + 48);
            animator.DrawSprite(&drawPos, true);
        }

        // Selection Line
        inkEffect        = INK_NONE;
        animator.frameID = 5;
        drawPos.x        = TO_FIXED(spritePosX);
        animator.DrawSprite(&drawPos, true);

        // Selection Player
        drawPos.x = TO_FIXED(position.x + 48);
        selectionAnimator.DrawSprite(&drawPos, true);
    }

    // Exit
    drawPos.y += TO_FIXED(48);
    if (selectedButton != 3) {
        inkEffect        = INK_ALPHA;
        animator.frameID = 9;
        drawPos.x        = TO_FIXED(position.x + 48);
        animator.DrawSprite(&drawPos, true);
    }
    // Exit (Selected)
    else {
        // Exit flash
        if (timer < 2) {
            inkEffect        = INK_NONE;
            animator.frameID = 9;
            drawPos.x        = TO_FIXED(position.x + 48);
            animator.DrawSprite(&drawPos, true);
        }

        // Selection Line
        inkEffect        = INK_NONE;
        animator.frameID = 5;
        drawPos.x        = TO_FIXED(spritePosX);
        animator.DrawSprite(&drawPos, true);

        // Selection Player
        drawPos.x = TO_FIXED(position.x + 48);
        selectionAnimator.DrawSprite(&drawPos, true);
    }
}

void PauseMenu::Draw_ResumeGame()
{
    SET_CURRENT_STATE();

    inkEffect = INK_NONE;

    // Background fade
    Graphics::DrawRect(NULL, NULL, screenInfo->size.x, pauseBarHeight, 0x000000, alpha, INK_ALPHA, true);

    // 'PAUSE' bar
    DrawPauseRect(pauseBarPos.y);

    animator.frameID = 3;
    drawPos.x        = TO_FIXED(position.x);
    drawPos.y        = 0;
    DrawSpikes();

    switch (selectedButton) {
        case 0: // Continue
            animator.frameID = 6;
            drawPos.x        = TO_FIXED(spritePosX);
            drawPos.y        = TO_FIXED(48);
            animator.DrawSprite(&drawPos, true);

            drawPos.x = TO_FIXED(position.x + 48);
            selectionAnimator.DrawSprite(&drawPos, true);
            break;
        case 1: // Reset
            animator.frameID = 7;
            drawPos.x        = TO_FIXED(spritePosX);
            drawPos.y        = TO_FIXED(96);
            animator.DrawSprite(&drawPos, true);

            drawPos.x = TO_FIXED(position.x + 48);
            selectionAnimator.DrawSprite(&drawPos, true);
            break;
        case 2: // Options
            animator.frameID = 8;
            drawPos.x        = TO_FIXED(spritePosX);
            drawPos.y        = TO_FIXED(144);
            animator.DrawSprite(&drawPos, true);

            drawPos.x = TO_FIXED(position.x + 48);
            selectionAnimator.DrawSprite(&drawPos, true);
            break;
        case 3: // Exit
            animator.frameID = 9;
            drawPos.x        = TO_FIXED(spritePosX);
            drawPos.y        = TO_FIXED(192);
            animator.DrawSprite(&drawPos, true);

            drawPos.x = TO_FIXED(position.x + 48);
            selectionAnimator.DrawSprite(&drawPos, true);
            break;
    }
}

void PauseMenu::Draw_ExitLevel()
{
    SET_CURRENT_STATE();

    inkEffect = INK_NONE;

    // Background fade
    Graphics::DrawRect(NULL, NULL, screenInfo->size.x, pauseBarHeight, 0x000000, backgroundAlpha, INK_ALPHA, true);

    // 'PAUSE' bar
    DrawPauseRect(202);

    animator.frameID = 3;
    drawPos.x        = TO_FIXED(position.x);
    drawPos.y        = 0;
    DrawSpikes();

    // The rectangle that consumes all life
    Graphics::DrawRect(position.x + 128, 0, screenInfo->size.x * 47, screenInfo->size.y, 0x000000, 0xFF, INK_NONE, true);

    drawPos.x = TO_FIXED(spritePosX);
    switch (selectedButton) {
        case 0: // Continue
            animator.frameID = 6;
            drawPos.y        = TO_FIXED(48);
            animator.DrawSprite(&drawPos, true);
            selectionAnimator.DrawSprite(&drawPos, true);
            break;
        case 1: // Reset
            animator.frameID = 7;
            drawPos.y        = TO_FIXED(96);
            animator.DrawSprite(&drawPos, true);
            selectionAnimator.DrawSprite(&drawPos, true);
            break;
        case 2: // Options
            animator.frameID = 8;
            drawPos.y        = TO_FIXED(144);
            animator.DrawSprite(&drawPos, true);
            selectionAnimator.DrawSprite(&drawPos, true);
            break;
        case 3: // Exit
            animator.frameID = 9;
            drawPos.y        = TO_FIXED(192);
            animator.DrawSprite(&drawPos, true);
            selectionAnimator.DrawSprite(&drawPos, true);
            break;
    }
}

void PauseMenu::StaticLoad(Static *sVars) { RSDK_INIT_STATIC_VARS(PauseMenu); }

void PauseMenu::Serialize() {}

} // namespace GameLogic