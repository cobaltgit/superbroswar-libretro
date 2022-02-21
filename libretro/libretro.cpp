#include "libretro.h"

#define VIDEO_WIDTH 640
#define VIDEO_HEIGHT 480
#define VIDEO_PIXELS VIDEO_WIDTH * VIDEO_HEIGHT

#define TITLESTRING "Super Mario War"
#define VERSIONNUMBER "2.0"

#include "CmdArgs.h"
#include "FileList.h"
#include "GameMode.h"
#include "gamemodes.h"
#include "GameValues.h"
#include "map.h"
#include "MapList.h"
#include "net.h"
#include "linfunc.h"
#include "player.h"
#include "ResourceManager.h"
#include "sfx.h"
#include "TilesetManager.h"

#include "FPSLimiter.h"
#include "GSSplashScreen.h"

#include <ctime>
#include <cmath>
#include <cstdlib> // srand()

//------ system stuff ------
SDL_Surface		*screen = NULL;		//for gfx (maybe the gfx system should be improved -> resource manager)
SDL_Surface		*blitdest = NULL;	//the destination surface for all drawing (can be swapped from screen to another surface)

short			x_shake = 0;
short			y_shake = 0;


//------ game relevant stuff ------
CPlayer			*list_players[4];
short			list_players_cnt = 0;

CScore			*score[4];

short			score_cnt;


//Locations for swirl spawn effects
short g_iSwirlSpawnLocations[4][2][25];


CGameMode			*gamemodes[GAMEMODE_LAST];
CGM_Bonus			*bonushousemode = NULL;
CGM_Pipe_MiniGame	*pipegamemode = NULL;
CGM_Boss_MiniGame	*bossgamemode = NULL;
CGM_Boxes_MiniGame	*boxesgamemode = NULL;

short currentgamemode = 0;

//Adds music overrides to the music lists
extern void UpdateMusicWithOverrides();

extern SDL_Joystick     **joysticks;
extern short            joystickcount;

extern short g_iDefaultPowerupPresets[NUM_POWERUP_PRESETS][NUM_POWERUPS];
extern short g_iCurrentPowerupPresets[NUM_POWERUP_PRESETS][NUM_POWERUPS];

extern CMap* g_map;
extern CTilesetManager* g_tilesetmanager;

extern FiltersList* filterslist;
extern MapList* maplist;
extern SkinList* skinlist;
extern AnnouncerList* announcerlist;
extern MusicList* musiclist;
extern WorldMusicList* worldmusiclist;
extern GraphicsList* menugraphicspacklist;
extern GraphicsList* worldgraphicspacklist;
extern GraphicsList* gamegraphicspacklist;
extern SoundsList* soundpacklist;
extern TourList* tourlist;
extern WorldList* worldlist;

extern std::string RootDataDirectory;
extern CGameValues game_values;
extern CResourceManager* rm;

static struct retro_log_callback logging;
static retro_log_printf_t log_cb;
static retro_video_refresh_t video_cb;
static retro_audio_sample_t audio_cb;
static retro_audio_sample_batch_t audio_batch_cb;
static retro_environment_t environ_cb;
static retro_input_poll_t input_poll_cb;
static retro_input_state_t input_state_cb;

void retro_set_video_refresh(retro_video_refresh_t cb) { video_cb = cb; }
void retro_set_audio_sample(retro_audio_sample_t cb) { audio_cb  =cb; }
void retro_set_audio_sample_batch(retro_audio_sample_batch_t cb) { audio_batch_cb = cb; }
void retro_set_input_poll(retro_input_poll_t cb) { input_poll_cb = cb; }
void retro_set_input_state(retro_input_state_t cb) { input_state_cb = cb; }

char retro_game_path[4096];

extern "C" short int libretro_input_state_cb(unsigned port, unsigned device, unsigned index, unsigned id)
{
    if (input_state_cb == NULL)
        return 0;
    
    return input_state_cb(port, device, index, id);
}

extern "C" void libretro_audio_cb(int16_t *buffer, uint32_t buffer_len)
{
    if (audio_batch_cb == NULL)
        return;
    
    audio_batch_cb(buffer, buffer_len);
}

static void create_globals()
{
    // this instance will contain the other relevant objects
    smw = new CGame(RootDataDirectory.c_str());
    rm = new CResourceManager();
#pragma warning ("delete these or use boost GC shared_ptr")

    g_map = new CMap();
    g_tilesetmanager = new CTilesetManager();

    filterslist = new FiltersList();
    maplist = new MapList(false);

    //TODO: add proper test via size
    if (maplist->IsEmpty()) {
        throw "Empty map directory!";
    }

    skinlist = new SkinList();
    musiclist = new MusicList();
    worldmusiclist = new WorldMusicList();
    soundpacklist = new SoundsList();
    announcerlist = new AnnouncerList();
    tourlist = new TourList();
    worldlist = new WorldList();

    menugraphicspacklist = new GraphicsList();
    worldgraphicspacklist = new GraphicsList();
    gamegraphicspacklist = new GraphicsList();

    announcerlist->SetCurrent(0);
    musiclist->SetCurrent(0);
    worldmusiclist->SetCurrent(0);
    menugraphicspacklist->SetCurrent(0);
    worldgraphicspacklist->SetCurrent(0);
    gamegraphicspacklist->SetCurrent(0);
    soundpacklist->SetCurrent(0);
}

void init_joysticks()
{
    SDL_InitSubSystem(SDL_INIT_JOYSTICK);
    joystickcount = (short)SDL_NumJoysticks();
    joysticks = new SDL_Joystick*[joystickcount];

    for (short i = 0; i < joystickcount; i++)
        joysticks[i] = SDL_JoystickOpen(i);

    SDL_JoystickEventState(SDL_ENABLE);
}

static void create_gamemodes()
{
    //set game modes
    gamemodes[0] = new CGM_Classic();
    gamemodes[1] = new CGM_Frag();
    gamemodes[2] = new CGM_TimeLimit();
    gamemodes[3] = new CGM_Jail();
    gamemodes[4] = new CGM_Coins();
    gamemodes[5] = new CGM_Stomp();
    gamemodes[6] = new CGM_Eggs();
    gamemodes[7] = new CGM_CaptureTheFlag();
    gamemodes[8] = new CGM_Chicken();
    gamemodes[9] = new CGM_Tag();
    gamemodes[10] = new CGM_Star();
    gamemodes[11] = new CGM_Domination();
    gamemodes[12] = new CGM_KingOfTheHill();
    gamemodes[13] = new CGM_Race();
    gamemodes[14] = new CGM_Owned();
    gamemodes[15] = new CGM_Frenzy();
    gamemodes[16] = new CGM_Survival();
    gamemodes[17] = new CGM_Greed();
    gamemodes[18] = new CGM_Health();
    gamemodes[19] = new CGM_Collection();
    gamemodes[20] = new CGM_Chase();
    gamemodes[21] = new CGM_ShyGuyTag();

    currentgamemode = 0;
    game_values.gamemode = gamemodes[currentgamemode];

    //Special modes
    bonushousemode = new CGM_Bonus();
    pipegamemode = new CGM_Pipe_MiniGame();
    bossgamemode = new CGM_Boss_MiniGame();
    boxesgamemode = new CGM_Boxes_MiniGame();
}

static void init_spawnlocations()
{
        //Calculate the swirl spawn effect locations
    float spawnradius = 100.0f;
    float spawnangle = 0.0f;

    for (short i = 0; i < 25; i++) {
        g_iSwirlSpawnLocations[0][0][i] = (short)(spawnradius * cos(spawnangle));
        g_iSwirlSpawnLocations[0][1][i] = (short)(spawnradius * sin(spawnangle));

        float angle = spawnangle + HALF_PI;
        g_iSwirlSpawnLocations[1][0][i] = (short)(spawnradius * cos(angle));
        g_iSwirlSpawnLocations[1][1][i] = (short)(spawnradius * sin(angle));

        angle = spawnangle + PI;
        g_iSwirlSpawnLocations[2][0][i] = (short)(spawnradius * cos(angle));
        g_iSwirlSpawnLocations[2][1][i] = (short)(spawnradius * sin(angle));

        angle = spawnangle + THREE_HALF_PI;
        g_iSwirlSpawnLocations[3][0][i] = (short)(spawnradius * cos(angle));
        g_iSwirlSpawnLocations[3][1][i] = (short)(spawnradius * sin(angle));

        spawnradius -= 4.0f;
        spawnangle += 0.1f;
    }
}

static void game_init()
{
    create_globals();

    printf("-------------------------------------------------------------------------------\n");
    printf(" %s %s\n", TITLESTRING, VERSIONNUMBER);
    printf("-------------------------------------------------------------------------------\n");
    printf("\n---------------- startup ----------------\n");

	gfx_init(smw->ScreenWidth, smw->ScreenHeight, false);		//initialize the graphics (SDL)
    blitdest = screen;

#if	0
    //Comment this in to performance test the preview map loading
    MI_MapField * miMapField = new MI_MapField(&rm->spr_selectfield, 70, 165, "Map", 500, 120);

    for (int k = 0; k < 100; k++) {
        game_values.playerInput.outputControls[3].menu_right.fPressed = true;
        miMapField->SendInput(&game_values.playerInput);
        //printf("Map over-> %s\n", maplist->currentFilename());
    }

    return 0;
#endif

    sfx_init();                     //init the sound system
    net_init();                     //init the networking

    init_joysticks();

    //currently this only sets the title, not the icon.
    //setting the icon isn't implemented in sdl ->  i'll ask on the mailing list
    char title[128];
    sprintf(title, "%s %s", TITLESTRING, VERSIONNUMBER);
    gfx_settitle(title);
    SDL_ShowCursor(SDL_DISABLE);

    printf("\n---------------- loading ----------------\n");

    for (short iScore = 0; iScore < 4; iScore++)
        score[iScore] = new CScore(iScore);

    game_values.init();

    //Set the default powerup weights for bonus wheel and [?] boxes
    for (short iPreset = 0; iPreset < NUM_POWERUP_PRESETS; iPreset++) {
        for (short iPowerup = 0; iPowerup < NUM_POWERUPS; iPowerup++) {
            g_iCurrentPowerupPresets[iPreset][iPowerup] = g_iDefaultPowerupPresets[iPreset][iPowerup];
        }
    }

    UpdateMusicWithOverrides();

    create_gamemodes();

    game_values.ReadBinaryConfig();

    //Assign the powerup weights to the selected preset
    for (short iPowerup = 0; iPowerup < NUM_POWERUPS; iPowerup++) {
        game_values.powerupweights[iPowerup] = g_iCurrentPowerupPresets[game_values.poweruppreset][iPowerup];
    }

    if (game_values.fullscreen) {
        gfx_changefullscreen(true);
        blitdest = screen;
    }

    init_spawnlocations();

    //Load the gfx color palette
    gfx_loadpalette(convertPathCP("gfx/packs/palette.bmp", gamegraphicspacklist->current_name()));

    //Call to setup input optimization
    game_values.playerInput.CheckIfMouseUsed();

    srand((unsigned int)time(NULL));

    SplashScreenState::instance().init();
    GameStateManager::instance().currentState = &SplashScreenState::instance();
}

static void game_deinit()
{
    printf("\n---------------- shutdown ----------------\n");

    for (short i = 0; i < GAMEMODE_LAST; i++)
        delete gamemodes[i];

    sfx_close();
    gfx_close();
    net_close();

    //Delete player skins
    for (short k = 0; k < MAX_PLAYERS; k++) {
        for (short j = 0; j < PGFX_LAST; j++) {
            delete rm->spr_player[k][j];
            delete rm->spr_shyguy[k][j];
            delete rm->spr_chocobo[k][j];
            delete rm->spr_bobomb[k][j];
        }

        delete [] rm->spr_player[k];
        delete [] rm->spr_shyguy[k];
        delete [] rm->spr_chocobo[k];
        delete [] rm->spr_bobomb[k];

        delete score[k];
    }

    delete [] game_values.pfFilters;
    delete [] game_values.piFilterIcons;

	// release all resources
	delete rm;
    delete smw;
}

static void gameloop_frame()
{
    FPSLimiter::instance().frameStart();

    GameStateManager::instance().currentState->update();

    FPSLimiter::instance().beforeFlip();
    gfx_flipscreen();
    FPSLimiter::instance().afterFlip();
}

void retro_init(void)
{
    input_state_cb = NULL;
    audio_batch_cb = NULL;

    SDL_putenv("SDL_VIDEODRIVER=dummy");
}

void retro_deinit(void)
{
    input_state_cb = NULL;
    audio_batch_cb = NULL;
}

unsigned retro_api_version(void)
{
    return RETRO_API_VERSION;
}

void retro_set_controller_port_device(unsigned port, unsigned device)
{
    log_cb(RETRO_LOG_INFO, "Plugging device %u into port %u.\n", device, port);
}

void retro_get_system_info(struct retro_system_info *info)
{
    memset(info, 0, sizeof(*info));
    info->library_name     = "Super Mario War";
    info->library_version  = "0.1";
    info->need_fullpath    = true;
    info->valid_extensions = "game";
}

void retro_get_system_av_info(struct retro_system_av_info *info)
{
    info->geometry.base_width   = VIDEO_WIDTH;
    info->geometry.base_height  = VIDEO_HEIGHT;
    info->geometry.max_width    = VIDEO_WIDTH;
    info->geometry.max_height   = VIDEO_HEIGHT;
    info->geometry.aspect_ratio = (640.0f / 480.0f);
    info->timing.fps            = 60.0f;
    info->timing.sample_rate    = 44100.0f;
}

void retro_set_environment(retro_environment_t cb)
{
    environ_cb = cb;
    bool no_content = true;

    if (cb(RETRO_ENVIRONMENT_GET_LOG_INTERFACE, &logging))
        log_cb = logging.log;

    cb(RETRO_ENVIRONMENT_SET_SUPPORT_NO_GAME, &no_content);
}

void retro_reset(void)
{

}

void retro_run(void)
{
    input_poll_cb();
    gameloop_frame();
    video_cb(screen->pixels, screen->w, screen->h, screen->pitch);
}

static void extract_directory(char *buf, const char *path, size_t size)
{
   strncpy(buf, path, size - 1);
   buf[size - 1] = '\0';

   char *base = strrchr(buf, '/');
   if (!base)
      base = strrchr(buf, '\\');

   if (base)
      *base = '\0';
   else
      buf[0] = '\0';
}

bool retro_load_game(const struct retro_game_info *info)
{
    struct retro_input_descriptor desc[] = {
        { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_LEFT,  "Left" },
        { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_UP,    "Up" },
        { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_DOWN,  "Down" },
        { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_RIGHT, "Right" },
        { 0 },
    };

    environ_cb(RETRO_ENVIRONMENT_SET_INPUT_DESCRIPTORS, desc);

    enum retro_pixel_format fmt = RETRO_PIXEL_FORMAT_RGB565;
    if (!environ_cb(RETRO_ENVIRONMENT_SET_PIXEL_FORMAT, &fmt))
    {
        log_cb(RETRO_LOG_INFO, "RGB565 is not supported.\n");
        return false;
    }

    extract_directory(retro_game_path, info->path, sizeof(retro_game_path));
    RootDataDirectory = std::string(retro_game_path);

    game_init();

    (void)info;
    return true;
}

void retro_unload_game(void)
{
    game_deinit();
}

unsigned retro_get_region(void)
{
    return RETRO_REGION_PAL;
}

bool retro_load_game_special(unsigned type, const struct retro_game_info *info, size_t num)
{
    return false;
}

size_t retro_serialize_size(void)
{
    return 0;
}

bool retro_serialize(void *data_, size_t size)
{
    return false;
}

bool retro_unserialize(const void *data_, size_t size)
{
    return false;
}

void *retro_get_memory_data(unsigned id)
{
    (void)id;
    return NULL;
}

size_t retro_get_memory_size(unsigned id)
{
    (void)id;
    return 0;
}

void retro_cheat_reset(void)
{}

void retro_cheat_set(unsigned index, bool enabled, const char *code)
{
    (void)index;
    (void)enabled;
    (void)code;
}