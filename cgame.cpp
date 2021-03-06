#include "shared.h"
#include "client.h"
#include "render.h"
#include <mmsystem.h>
#pragma comment(lib, "winmm.lib")

typedef enum {
	CG_R_ADDREFENTITYTOSCREEN = 61,

	CG_GETDOBJ = 162,









	e_cgame_trap_end
} e_cgame_trap;

/* notepad ++ search for regex for e.g more parms with specific syscall num with
_30074898\([^,]*, 61
*/

typedef void(*CG_ServerCommand_t)();
CG_ServerCommand_t CG_ServerCommand;

const char* disliked_vars[] = { 
"r_showimages",
"name",
"cl_allowdownload",
"version",
"cg_norender",
"cl_avidemo",
NULL };

DWORD cgame_mp;

class ChatMessage {
public:
	std::string msg;
	time_t addtime;

	ChatMessage() {
		addtime = time(NULL);
	}
};

std::vector<ChatMessage> chatmessages;

void RGL_DrawChatText(std::string &msg, int x, int y) {
	glRasterPos2f(x, y);
	PrintFont(fontIngameChatMessage, msg.c_str());
}

void CG_RenderChatMessages() {
	cvar_t *x = Cvar_Get("cg_xui_chat_x", "20", CVAR_ARCHIVE);
	cvar_t *y = Cvar_Get("cg_xui_chat_y", "60", CVAR_ARCHIVE);
	int n = 0;
	for (auto &i : chatmessages) {
		RGL_DrawChatText(i.msg, x->integer, y->integer + 14 * n);
		++n;
	}
}

void CG_RemoveChatEscapeChar(char *s) {
	char *src, *dest;
	for (src = s, dest = s; *src; src++) {
		if( *src == '^' && isdigit(*(src + 1)) || isdigit(*src) && src != s && *(src - 1) == '^')
			continue;
		if (*src != 25 && *src >= 32 && *src <= 126)
			*dest++ = *src;
	}
	*dest = 0;
}

char *(*CG_Argv)(int) = nullptr;

void myCG_ServerCommand(void) {
	int argc = Cmd_Argc();
	#if 0
		Com_Printf("^2CG_ServerCommand: ");
		for (int i = 0; i < argc; i++)
			Com_Printf("%s ", Cmd_Argv(i));
		Com_Printf("\n");
	#endif

	if (argc > 0) {
		char* cmd = Cmd_Argv(0);
		if (strlen(cmd) > 0) {
			cvar_t *xui_alt_chat = Cvar_Get("cg_xui_chat", "0", CVAR_ARCHIVE);
			if ((*cmd == 'h' || *cmd == 'i')) {
				if (xui_alt_chat->integer) {
					if (*cmd == 'i' || (*cmd == 'h' && !*(int*)CGAME_OFF(0x3029824C))) {
						ChatMessage cmsg;
						if (!strlen(Cmd_Argv(1)))
							return;
						char msg[150] = { 0 };
						Q_strncpyz(msg, Cmd_Argv(1), sizeof(msg));

						CG_RemoveChatEscapeChar(msg);

						cmsg.msg = msg;
						if (chatmessages.size() > 8) {
							chatmessages.clear();
						}
						chatmessages.push_back(cmsg);

						return;
					}
				}
				if (xui_alt_chat->modified)
					chatmessages.clear();
			} else if (*cmd == 'b') {
				Com_DPrintf("[CG_ParseScores] b ");
				for (size_t i = 0; i < argc; i++) {
					Com_DPrintf("%s ", Cmd_Argv(i));
				}
				Com_DPrintf("\n");
			} else if (*cmd == 'v') {
				if (argc > 1) {
					char* var = Cmd_Argv(1);

					for (int i = 0; disliked_vars[i]; i++) {
						if (!strcmp(disliked_vars[i], var))
							return; // kindly fuck off please (c) php
					}
				}

			}
		}
	}
	CG_ServerCommand();
}

void pm_aimflag() {
	int *pm = (int*)(cgame_mp + 0x19D570);
	int *ps = (int*)*pm;
	int *gclient = (int*)*ps;

	int *v4 = (int *)(ps + 12);

	int val = *(int*)(gclient + 21); //336? 84*4=336 /84/4=21??

	if (val == 1023) {
		*v4 |= 0x20;
		return;
	}

	void(*call)();
	*(int*)&call = CGAME_OFF(0x3000FB80);
	call();
}

#define cg_crosshairClientNum (*(int*)CGAME_OFF(0x3020C8C8))
#define cg_renderingThirdPerson (*(int*)CGAME_OFF(0x30207158))

typedef enum {
	TEAM_FREE,
	TEAM_AXIS,
	TEAM_ALLIES,
	TEAM_SPECTATOR,

	TEAM_NUM_TEAMS
} team_t;

static const char *teamStrings[] = { "TEAM_FREE", "Axis", "Allies", "Spectator" };

typedef struct {
	int snapsFlags;
	int ping;
	int serverTime;
	//rest
} snapshot_t;

typedef struct {
	int unk;
	int infoValid;
	int clientNum;
	char name[32];
	team_t team;
	//down here model names and attachments and rest of client info
} clientInfo_t;

typedef struct {
	float alpha;
	int clientNum;
	time_t time;
	team_t team;
} fadeStatus_t;

#define MAX_LAST_NUMS 5
static fadeStatus_t lastNums[MAX_LAST_NUMS] = { 0 };
static int lastNumsIndex = 0;

static clientInfo_t *other_info = NULL, *info = NULL;
static int *cent;

#define CLIENTINFO_SIZE 0x448

void R_AddDebugString(float *origin, float *color, float fontScale, const char* str) {
	__asm {
		push str
			push fontScale
			mov ebx, origin
			mov edi, color
			mov eax, 0x4E0A40
			call eax
			add esp, 8
	}
}

void CG_DrawCrosshairNames() {
	cg_crosshairClientNum = 65;
	float(*CG_ScanForCrosshairEntity)();
	*(int*)&CG_ScanForCrosshairEntity = CGAME_OFF(0x30016E50);
	CG_ScanForCrosshairEntity();

	if (cg_crosshairClientNum > 64)
		return;

	other_info = (clientInfo_t*)(0x448 * cg_crosshairClientNum + CGAME_OFF(0x3018BC0C));
	info = (clientInfo_t*)(0x448 * *(int*)(*(int*)CGAME_OFF(0x301E2160) + 184) + CGAME_OFF(0x3018BC0C));

	if (other_info->team == TEAM_ALLIES || other_info->team == TEAM_AXIS) {
		if (!*(int*)CGAME_OFF(0x3020BA24) && !*(int*)CGAME_OFF(0x3020BA28)) {
			return;
		}
		else if (other_info->team == info->team || info->team == TEAM_SPECTATOR) {
			for (int i = 0; i < MAX_LAST_NUMS; i++) {
				if (lastNums[i].alpha > 0 && lastNums[i].clientNum == cg_crosshairClientNum) {
					lastNums[i].alpha = 1;
					return;
				}
			}

			lastNumsIndex = (lastNumsIndex + 1) % MAX_LAST_NUMS;
			fadeStatus_t *st = &lastNums[lastNumsIndex];
			st->clientNum = cg_crosshairClientNum;
			st->time = *cls_realtime;
			st->alpha = 1;
			st->team = other_info->team;

			//float *origin = (float*)FILE_OFF(0x301E2188); //my origin
			//R_AddDebugString(origin,color,2,other_info->name);
		}
	}
}

void __cdecl cg_playersprites_sub() {

	other_info = NULL;

	for (int i = 0; i < MAX_LAST_NUMS; i++) {
		fadeStatus_t *st = &lastNums[i];

		if (st->clientNum != *(int*)((int)cent + 144))
			continue;

		if (st->alpha <= 0) {
			st->time = 0;
			st->alpha = 0;
			st->clientNum = 65;
			continue;
		}

		if (*cls_realtime - st->time > 350) {
			st->alpha -= 0.1;
			st->time = *cls_realtime;
		}

		other_info = (clientInfo_t*)(0x448 * st->clientNum + CGAME_OFF(0x3018BC0C));

#ifndef SMALLCHAR_WIDTH
#define SMALLCHAR_WIDTH 48
#endif
		vec4_t color = { 1, 1, 1, st->alpha };
		vec3_t org = { 0 };
		org[0] = *(float*)((int)cent + 504);
		org[1] = *(float*)((int)cent + 508);
		org[2] = *(float*)((int)cent + 512) + 80;
		char tmp_name[64] = { 0 };
		strcpy(tmp_name, other_info->name);
		R_AddDebugString(org, color, .5, Q_CleanStr(tmp_name));
	}
}

void(*CG_PlayerSprites)();
void _CG_PlayerSprites() {
	__asm mov cent, eax
	CG_PlayerSprites();
	cg_playersprites_sub();
}

void CG_SetHeadNames(int flag) {
	if (!cgame_mp)
		return;

	if (*cls_state != 6) // CA_ACTIVE
		return;

	static char org_bytes[2][5] = {
		{ 0xA1, 0x2C, 0x90, 0x1D, 0x30 },
		{ 0xE8, 0xED, 0xF1, 0xFF, 0xFF }
	};
	if (!flag) {
		_memcpy((void*)CGAME_OFF(0x30016F70), &org_bytes[0], 5);
		_memcpy((void*)CGAME_OFF(0x300282DE), &org_bytes[1], 5);
		return;
	}
	__jmp(CGAME_OFF(0x30016F70), (int)CG_DrawCrosshairNames);
	__call(CGAME_OFF(0x300282DE), (int)_CG_PlayerSprites);
}

void ParseVector(char *s, vec3_t out) {
	if (!s || !*s)
		return;

	if (strlen(s) > 200)
		return;
	char b[128] = { 0 };
	int outN = 0, bi = 0;
	for (int i = 0; s[i] != '\0'; i++) {
		if ((i != 0 && s[i] == ' ') || s[i + 1] == '\0') {
			out[outN++] = atof(b);
			memset(b, 0, sizeof(b));
			bi = 0;
		}
		else {
			b[bi++] = s[i];
		}

	}
}

char *PrintVector(vec3_t v) {
	return va("x: %f, y: %f, z: %f", v[0], v[1], v[2]);
}

#define M_DrawShadowedString(x, y, fontscale, fontcolor, text) \
	SCR_DrawString(x+2,y+2, 1, fontscale, vColorBlack, text, NULL, NULL, NULL); \
	SCR_DrawString(x,y,1,fontscale,fontcolor,text,NULL,NULL,NULL);


#pragma pack(push, 1)
typedef struct {
	int clientNum;
	int score;
	int ping;
	int deaths;
	char *clientName;
	int statusicon;
} clientScoreInfo_t;
#pragma pack(pop)

#include <algorithm>

int scoreboardScrollAmount = 0;

bool showScoreboard = false;

void CG_KeyEvent(int key, int down, unsigned time) {
	if (keys[key].repeats > 1)
		return;
	if (!down)
		return;

	if (!cgame_mp)
		return;

	if (!showScoreboard)
		return;

	if (key == K_MWHEELDOWN) {
		scoreboardScrollAmount+=5;
	} else if (key == K_MWHEELUP) {
		if (scoreboardScrollAmount - 5 >= 0)
		scoreboardScrollAmount-=5;
	}
}


int CG_DrawScoreboard() {
	int *showScores = (int*)CGAME_OFF(0x3020C030);

	if (!*showScores) {
		showScoreboard = false;
		__asm xor eax, eax
		return 0;
	}
	showScoreboard = true;
#define BASE_Y 80
#define BANNER_SIZE 40
#define BANNER_WIDTH 372

#define BAR_OPACITY .2
#define BAR_HEIGHT 15
#define BAR_WIDTH 320 + 45
#define BAR_PAD 5

	int base_x = 160 - 50;
	int base_end_x = BAR_WIDTH + base_x;
	int base_y = BASE_Y - scoreboardScrollAmount;

	vec4_t teamcolor_allies = { 1, 1, 1, BAR_OPACITY };
	vec4_t teamcolor_axis = { 1, 1, 1, BAR_OPACITY };
	vec4_t teamcolor_none = { .5, .5, .5, BAR_OPACITY };

	ParseVector(Cvar_VariableString("g_TeamColor_Allies"), teamcolor_allies);
	ParseVector(Cvar_VariableString("g_TeamColor_Axis"), teamcolor_axis);

	char *teamname_allies = Cvar_VariableString("g_TeamName_Allies"); 
	char *teamname_axis = Cvar_VariableString("g_TeamName_Axis");

	if (strstr(teamname_allies, "AMERICAN") != NULL)
		Cvar_Set("g_TeamName_Allies", "American");
	else if (strstr(teamname_allies, "RUSSIAN") != NULL)
		Cvar_Set("g_TeamName_Allies", "Russian");
	else if (strstr(teamname_allies, "BRITISH") != NULL)
		Cvar_Set("g_TeamName_Allies", "British");

	if (strstr(teamname_axis, "GERMAN") != NULL)
		Cvar_Set("g_TeamName_Axis", "German");

	if (!*teamname_allies)
		teamname_allies = "Allies";
	if (!*teamname_axis)
		teamname_axis = "Axis";

	int num_allies = *(int*)CGAME_OFF(0x3020BA24);
	int num_axis = *(int*)CGAME_OFF(0x3020BA28);

	int num_players = *(int*)CGAME_OFF(0x3020B9FC);
	int num_spectators = *(int*)CGAME_OFF(0x3020BA2C);

	clientScoreInfo_t *clientscoreinfo = (clientScoreInfo_t*)CGAME_OFF(0x3020BA30);
	clientInfo_t *ci;

	std::vector<clientScoreInfo_t> sortedscores;

	if (num_players < 0 || num_players >= 64)
		goto _end;

	#define GetClientInfo(i) \
		((clientInfo_t*)(CLIENTINFO_SIZE * i + CGAME_OFF(0x3018BC0C)))

	for (int i = 0; i < num_players; i++)
		sortedscores.push_back(clientscoreinfo[i]);

	struct pred {
		bool operator()(clientScoreInfo_t const & a, clientScoreInfo_t const & b) const {
			return a.score > b.score;
		}
	};

	std::sort(sortedscores.begin(), sortedscores.end(), pred());
#define COLUMN_WIDTH 40
#define M_DrawShadowString(x,y,font,fontscale,color,text,a,b,c) \
	RE_SetColor(vColorBlack); \
	SCR_DrawString(x + 1,y + 1,font,fontscale,vColorBlack,text,a,b,c); \
	RE_SetColor(color); \
	SCR_DrawString(x,y,font,fontscale,color,text,a,b,c); \
	RE_SetColor(NULL);

	if (num_players - num_spectators > 0) {
		if ((!num_allies || !num_axis)) { //draw deathmatch :>
			RE_SetColor(vColorWhite);
			if (base_y >= BASE_Y) {
				int g_ScoresBanner_None = RE_RegisterShaderNoMip(Cvar_VariableString("g_ScoresBanner_None"));

				SCR_DrawPic(base_x, base_y - BANNER_SIZE, BANNER_WIDTH, BANNER_SIZE, g_ScoresBanner_None);
				M_DrawShadowString(base_x + BANNER_SIZE, base_y, 1, .3, vColorWhite, va("Players ( %d )", num_players - num_spectators), NULL, NULL, NULL);
				M_DrawShadowString(base_end_x - COLUMN_WIDTH * 3, base_y, 1, .3, vColorWhite, Cvar_VariableString("g_scoreboard_kills"), NULL, NULL, NULL);
				M_DrawShadowString(base_end_x - COLUMN_WIDTH * 2, base_y, 1, .3, vColorWhite, Cvar_VariableString("g_scoreboard_deaths"), NULL, NULL, NULL);
				M_DrawShadowString(base_end_x - COLUMN_WIDTH + 15, base_y, 1, .3, vColorWhite, Cvar_VariableString("g_scoreboard_ping"), NULL, NULL, NULL);

				base_y += 5;
			}
			for (int i = 0; i < sortedscores.size(); i++) {
				clientScoreInfo_t *csi = &sortedscores[i];
				if (csi->clientNum < 0 || csi->clientNum >= 64)
					continue;
				ci = GetClientInfo(csi->clientNum);
				if (ci->name == nullptr)
					continue;
				if (ci->team == TEAM_SPECTATOR)
					continue;
				if ((base_y + BAR_HEIGHT + BAR_PAD) < BASE_Y) {
					base_y += BAR_HEIGHT + BAR_PAD;
					continue;
				}
				RE_SetColor(teamcolor_none);
				SCR_DrawPic(base_x, base_y, BAR_WIDTH, BAR_HEIGHT, RE_RegisterShader("white"));
				RE_SetColor(vColorWhite);
				if (csi->statusicon)
				SCR_DrawPic(base_x, base_y, BAR_HEIGHT, BAR_HEIGHT, csi->statusicon);
				M_DrawShadowString(base_x + BAR_HEIGHT , base_y + 15, 1, .3, vColorWhite, ci->name, NULL, NULL, NULL);
				M_DrawShadowString(base_end_x - COLUMN_WIDTH * 3 + 12, base_y + 15, 1, .3, vColorWhite, va("%d", csi->score), NULL, NULL, NULL);
				M_DrawShadowString(base_end_x - COLUMN_WIDTH * 2 + 12, base_y + 15, 1, .3, vColorWhite, va("%d", csi->deaths), NULL, NULL, NULL);
				M_DrawShadowString(base_end_x - COLUMN_WIDTH + 12, base_y + 15, 1, .3, vColorWhite, va("%d", csi->ping), NULL, NULL, NULL);
				base_y += BAR_HEIGHT + BAR_PAD;
			}
			RE_SetColor(NULL);
		} else {
			int allies_score = 0, axis_score = 0;

			for (int i = 0; i < num_players; i++) {
				clientScoreInfo_t *csi = &clientscoreinfo[i];
				ci = GetClientInfo(csi->clientNum);

				if (ci->team == TEAM_ALLIES)
					allies_score += csi->score;
				else if (ci->team = TEAM_AXIS)
					axis_score += csi->score;
			}
			char *teamname1, *teamname2;
			int teamcount1, teamcount2;
			float *teamcolor1, *teamcolor2;
			char *teambanner_n1, *teambanner_n2;
			int team1, team2;
			if (allies_score >= axis_score) {
				team1 = TEAM_ALLIES;
				team2 = TEAM_AXIS;
				teamname1 = teamname_allies;
				teamname2 = teamname_axis;
				teamcount1 = num_allies;
				teamcount2 = num_axis;
				teambanner_n1 = Cvar_VariableString("g_ScoresBanner_Allies");
				teambanner_n2 = Cvar_VariableString("g_ScoresBanner_Axis");
				teamcolor1 = teamcolor_allies;
				teamcolor2 = teamcolor_axis;
			} else {
				team1 = TEAM_AXIS;
				team2 = TEAM_ALLIES;
				teamname2 = teamname_allies;
				teamname1 = teamname_axis;
				teamcount2 = num_allies;
				teamcount1 = num_axis;
				teambanner_n2 = Cvar_VariableString("g_ScoresBanner_Allies");
				teambanner_n1 = Cvar_VariableString("g_ScoresBanner_Axis");
				teamcolor2 = teamcolor_allies;
				teamcolor1 = teamcolor_axis;
			}

				RE_SetColor(vColorWhite);
				if (base_y >= BASE_Y) {
					int banner1 = RE_RegisterShaderNoMip(teambanner_n1);

					SCR_DrawPic(base_x, base_y - BANNER_SIZE, BANNER_WIDTH, BANNER_SIZE, banner1);
					M_DrawShadowString(base_x + BANNER_SIZE, base_y, 1, .3, vColorWhite, va("%s ( %d )", teamname1, teamcount1), NULL, NULL, NULL);
					M_DrawShadowString(base_end_x - COLUMN_WIDTH * 3, base_y, 1, .15, vColorWhite, Cvar_VariableString("g_scoreboard_kills"), NULL, NULL, NULL);
					M_DrawShadowString(base_end_x - COLUMN_WIDTH * 2, base_y, 1, .15, vColorWhite, Cvar_VariableString("g_scoreboard_deaths"), NULL, NULL, NULL);
					M_DrawShadowString(base_end_x - COLUMN_WIDTH + 15, base_y, 1, .15, vColorWhite, Cvar_VariableString("g_scoreboard_ping"), NULL, NULL, NULL);

					base_y += 5;
				}
				for (int i = 0; i < sortedscores.size(); i++) {
					clientScoreInfo_t *csi = &sortedscores[i];
					if (csi->clientNum < 0 || csi->clientNum >= 64)
						continue;
					ci = GetClientInfo(csi->clientNum);
					if (ci->name == nullptr)
						continue;
					if (ci->team != team1)
						continue;
					if ((base_y + BAR_HEIGHT + BAR_PAD) < BASE_Y) {
						base_y += BAR_HEIGHT + BAR_PAD;
						continue;
					}
					RE_SetColor(teamcolor1);
					SCR_DrawPic(base_x, base_y, BAR_WIDTH, BAR_HEIGHT, RE_RegisterShader("white"));
					RE_SetColor(vColorWhite);
					if (csi->statusicon)
					SCR_DrawPic(base_x, base_y, BAR_HEIGHT, BAR_HEIGHT, csi->statusicon);
					M_DrawShadowString(base_x + BAR_HEIGHT , base_y + 10, 1, .15, vColorWhite, ci->name, NULL, NULL, NULL);
					M_DrawShadowString(base_end_x - COLUMN_WIDTH * 3 + 12, base_y + 10, 1, .15, vColorWhite, va("%d", csi->score), NULL, NULL, NULL);
					M_DrawShadowString(base_end_x - COLUMN_WIDTH * 2 + 12, base_y + 10, 1, .15, vColorWhite, va("%d", csi->deaths), NULL, NULL, NULL);
					M_DrawShadowString(base_end_x - COLUMN_WIDTH + 12, base_y + 10, 1, .15, vColorWhite, va("%d", csi->ping), NULL, NULL, NULL);
					base_y += BAR_HEIGHT + BAR_PAD;
				}
				RE_SetColor(NULL);

				base_y += 50;
				RE_SetColor(vColorWhite);
				if (base_y >= BASE_Y) {
					int banner2 = RE_RegisterShaderNoMip(teambanner_n2);

					SCR_DrawPic(base_x, base_y - BANNER_SIZE, BANNER_WIDTH, BANNER_SIZE, banner2);
					M_DrawShadowString(base_x + BANNER_SIZE, base_y, 1, .3, vColorWhite, va("%s ( %d )", teamname2, teamcount2), NULL, NULL, NULL);
					
					base_y += 5;
				}
				for (int i = 0; i < sortedscores.size(); i++) {
					clientScoreInfo_t *csi = &sortedscores[i];
					if (csi->clientNum < 0 || csi->clientNum >= 64)
						continue;
					ci = GetClientInfo(csi->clientNum);
					if (ci->name == nullptr)
						continue;
					if (ci->team != team2)
						continue;
					if ((base_y + BAR_HEIGHT + BAR_PAD) < BASE_Y) {
						base_y += BAR_HEIGHT + BAR_PAD;
						continue;
					}
					RE_SetColor(teamcolor2);
					SCR_DrawPic(base_x, base_y, BAR_WIDTH, BAR_HEIGHT, RE_RegisterShader("white"));
					RE_SetColor(vColorWhite);
					if (csi->statusicon)
					SCR_DrawPic(base_x, base_y, BAR_HEIGHT, BAR_HEIGHT, csi->statusicon);
					M_DrawShadowString(base_x + BAR_HEIGHT * 2, base_y + 10, 1, .15, vColorWhite, ci->name, NULL, NULL, NULL);
					M_DrawShadowString(base_end_x - COLUMN_WIDTH * 3 + 12, base_y + 10, 1, .15, vColorWhite, va("%d", csi->score), NULL, NULL, NULL);
					M_DrawShadowString(base_end_x - COLUMN_WIDTH * 2 + 12, base_y + 10, 1, .15, vColorWhite, va("%d", csi->deaths), NULL, NULL, NULL);
					M_DrawShadowString(base_end_x - COLUMN_WIDTH + 12, base_y + 10, 1, .15, vColorWhite, va("%d", csi->ping), NULL, NULL, NULL);
					base_y += BAR_HEIGHT + BAR_PAD;
				}
				RE_SetColor(NULL);
		}
	}

	vec4_t color_spectator_bar_bg = { 1, 1, 1, BAR_OPACITY };

	if (num_spectators > 0) {
		base_y += 50;
		RE_SetColor(vColorWhite);

		int g_ScoresBanner_Spectators = RE_RegisterShaderNoMip(Cvar_VariableString("g_ScoresBanner_Spectators"));
		if (base_y >= BASE_Y){
			SCR_DrawPic(base_x, base_y - BANNER_SIZE, BANNER_WIDTH, BANNER_SIZE, g_ScoresBanner_Spectators);
			M_DrawShadowString(base_x + BANNER_SIZE, base_y, 1, .3, vColorWhite, va("Spectators ( %d )", num_spectators), NULL, NULL, NULL);
		}
		for (int i = 0; i < sortedscores.size(); i++) {
			clientScoreInfo_t *csi = &sortedscores[i];
			if (csi->clientNum < 0 || csi->clientNum >= 64)
				continue;
			ci = GetClientInfo(csi->clientNum);
			if (ci->name == nullptr)
				continue;
			if (ci->team != TEAM_SPECTATOR && ci->team != TEAM_FREE)
				continue;
			if ((base_y + BAR_HEIGHT + BAR_PAD) < BASE_Y) {
				base_y += BAR_HEIGHT + BAR_PAD;
				continue;
			}
			RE_SetColor(color_spectator_bar_bg);
			SCR_DrawPic(base_x, base_y, BAR_WIDTH, BAR_HEIGHT, RE_RegisterShader("white"));
			RE_SetColor(vColorWhite);
			if (csi->statusicon)
			SCR_DrawPic(base_x, base_y, BAR_HEIGHT, BAR_HEIGHT, csi->statusicon);
			M_DrawShadowString(base_x + BAR_HEIGHT * 2, base_y + 15, 1, .3, vColorWhite, ci->name, NULL, NULL, NULL);
			base_y += BAR_HEIGHT + BAR_PAD;
		}
		RE_SetColor(NULL);
	}
	_end:
	__asm mov eax, 1
	return 1;
}

cJMP cj_scoreboard;

extern cvar_t *cg_drawheadnames;
extern cvar_t *cg_xui_scoreboard;

void CG_SCR_DrawScreenField(int stereoFrame) {
	if (cg_drawheadnames->modified) {
		void CG_SetHeadNames(int flag);
		CG_SetHeadNames(cg_drawheadnames->integer);
	}

	if (cg_xui_scoreboard->modified && *cls_state == 6) {
		if (cg_xui_scoreboard->integer)
			cj_scoreboard.Apply();
		else
			cj_scoreboard.Restore();
	}
}

void CG_DrawDisconnect() {
	cvar_t* xui_interrupted = Cvar_Get("cg_xui_interrupted", "0", CVAR_ARCHIVE);
	if (xui_interrupted->integer) {
		void(*call)();
		*(int*)&call = CGAME_OFF(0x30015450);
		call();
	}
}

void CG_DrawFPS(float y) {
	cvar_t* xui_fps = Cvar_Get("cg_xui_fps", "1", CVAR_ARCHIVE);

	if (xui_fps->integer) {
		cvar_t* x = Cvar_Get("cg_xui_fps_x", "597", CVAR_ARCHIVE); // uh this x y values just look good with my hp bar
		cvar_t* y = Cvar_Get("cg_xui_fps_y", "8", CVAR_ARCHIVE);

		#define	FPS_FRAMES 4
		static int previousTimes[FPS_FRAMES];
		static int index;
		int	i, total;
		int	fps;
		static int previous;
		int	t, frameTime;

		t = timeGetTime();
		frameTime = t - previous;
		previous = t;
		previousTimes[index % FPS_FRAMES] = frameTime;
		index++;

		if (index > FPS_FRAMES) {
			total = 0;
			for (i = 0; i < FPS_FRAMES; i++) {
				total += previousTimes[i];
			}
			if (!total) {
				total = 1;
			}
			fps = 1000 * FPS_FRAMES / total;

			M_DrawShadowString(x->integer, y->integer, 1, .20, vColorWhite, va("FPS: %d", fps), NULL, NULL, NULL);
		}
	} else {
		void(*call)(float);
		*(int*)&call = CGAME_OFF(0x30014A00);
		call(y);
	}
}

void CG_Init(DWORD base) {
	cgame_mp = base;
	CG_ServerCommand = (CG_ServerCommand_t)(cgame_mp + 0x2E0D0);
	CG_Argv = (char*(*)(int))CGAME_OFF(0x30020960);

	//__jmp(CGAME_OFF(0x3002B650), (int)CG_DrawScoreboard);
	cj_scoreboard.Initialize(CGAME_OFF(0x3002B650), (UINT32)CG_DrawScoreboard, true);

	XUNLOCK((void*)CGAME_OFF(0x3020C8C8), sizeof(int));

	XUNLOCK((void*)CGAME_OFF(0x3020C8C8), sizeof(int)); //crosshairnumber

	__call(CGAME_OFF(0x3002E5A6), (int)myCG_ServerCommand);
	__call(CGAME_OFF(0x3000C799), (int)pm_aimflag);
	__call(CGAME_OFF(0x3000C7B8), (int)pm_aimflag);
	__call(CGAME_OFF(0x3000C7D2), (int)pm_aimflag);
	__call(CGAME_OFF(0x3000C7FF), (int)pm_aimflag);
	__call(CGAME_OFF(0x3000C858), (int)pm_aimflag);
	__call(CGAME_OFF(0x3000C893), (int)pm_aimflag);
	CG_PlayerSprites = (void(*)())CGAME_OFF(0x300274D0);

	__call(CGAME_OFF(0x300159CC), (int)CG_DrawDisconnect);
	__call(CGAME_OFF(0x300159D4), (int)CG_DrawDisconnect);

	__call(CGAME_OFF(0x3001509E), (int)CG_DrawFPS);

	*(UINT32*)CGAME_OFF(0x300749EC) = 1; // Enable cg_fov
	// *(UINT32*)CGAME_OFF(0x30074EBC) = 0; // Enable cg_thirdperson

	void CG_SetHeadNames(int flag);
	CG_SetHeadNames(cg_drawheadnames->integer);
}
