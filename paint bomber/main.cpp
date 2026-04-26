#include "raylib.h"
#include "raymath.h"
#include "rlgl.h" 
#include <vector>

const int GRID_SIZE = 15;
const float TILE_SIZE = 4.0f;
const float GAME_TIME = 180.0f;

enum TileType { EMPTY, WALL_STEEL, WALL_WOOD };
enum Owner { NONE, PLAYER1, PLAYER2 };
struct Tile { TileType type; Owner owner; };
struct Bomb { Vector2 pos; Owner creator; };
struct Player { Vector3 position; Color color; Owner id; float stunTimer; int score; };

// --- Global Variables ---
Tile map[GRID_SIZE][GRID_SIZE];
std::vector<Bomb> bombs;
std::vector<Player*> playerPtrs; // ย้ายมาเป็น Global เพื่อให้ ExecuteExplosion เข้าถึงได้

// ฟังก์ชันช่วยวาดแผ่นสี่เหลี่ยมที่มี Texture ในโลก 3D (เสถียรกว่า DrawCubeTexture)
void DrawPlaneTexture(Texture2D texture, Vector3 center, float width, float length, Color color) {
    float x = center.x; float y = center.y; float z = center.z;
    float w2 = width / 2.0f; float l2 = length / 2.0f;
    rlSetTexture(texture.id);
    rlBegin(RL_QUADS);
    rlColor4ub(color.r, color.g, color.b, color.a);
    rlNormal3f(0.0f, 1.0f, 0.0f);
    rlTexCoord2f(0.0f, 0.0f); rlVertex3f(x - w2, y, z - l2);
    rlTexCoord2f(0.0f, 1.0f); rlVertex3f(x - w2, y, z + l2);
    rlTexCoord2f(1.0f, 1.0f); rlVertex3f(x + w2, y, z + l2);
    rlTexCoord2f(1.0f, 0.0f); rlVertex3f(x + w2, y, z - l2);
    rlEnd();
    rlSetTexture(0);
}

void InitMap() {
    for (int x = 0; x < GRID_SIZE; x++) {
        for (int z = 0; z < GRID_SIZE; z++) {
            map[x][z].owner = NONE;
            if (x == 0 || x == GRID_SIZE - 1 || z == 0 || z == GRID_SIZE - 1 || (x % 2 == 0 && z % 2 == 0)) map[x][z].type = WALL_STEEL;
            else if (GetRandomValue(0, 10) > 4) map[x][z].type = WALL_WOOD;
            else map[x][z].type = EMPTY;
        }
    }
    map[1][1].type = map[1][2].type = map[2][1].type = EMPTY;
    map[GRID_SIZE - 2][GRID_SIZE - 2].type = map[GRID_SIZE - 2][GRID_SIZE - 3].type = map[GRID_SIZE - 3][GRID_SIZE - 2].type = EMPTY;
}

void ExecuteExplosion(int bx, int by, Owner owner) {
    int range = 3;
    int dx[] = { 0, 0, 1, -1 };
    int dz[] = { 1, -1, 0, 0 };
    map[bx][by].owner = owner;
    for (int i = 0; i < 4; i++) {
        for (int r = 1; r <= range; r++) {
            int nx = bx + dx[i] * r;
            int nz = by + dz[i] * r;
            if (nx < 0 || nx >= GRID_SIZE || nz < 0 || nz >= GRID_SIZE) break;
            if (map[nx][nz].type == WALL_STEEL) break;

            map[nx][nz].owner = owner;
            // เช็คการโดนระเบิดของ Player ทุกคน
            for (auto& p : playerPtrs) {
                int px = (int)((p->position.x + TILE_SIZE / 2) / TILE_SIZE);
                int pz = (int)((p->position.z + TILE_SIZE / 2) / TILE_SIZE);
                if (px == nx && pz == nz) {
                    if (p->id != owner) p->stunTimer = 2.0f;
                }
            }
            if (map[nx][nz].type == WALL_WOOD) { map[nx][nz].type = EMPTY; break; }
        }
    }
}

int main() {
    SetConfigFlags(FLAG_FULLSCREEN_MODE);
    InitWindow(0, 0, "Remote Paint Bomber - Final Version");
    SetTargetFPS(60);
    InitMap();

    Texture2D p1Tex = LoadTexture("picture/dog.png");
    Texture2D p2Tex = LoadTexture("picture/cat.png");
    Texture2D metalTex = LoadTexture("picture/metal.png");
    Texture2D woodTex = LoadTexture("picture/wood.png");
    Texture2D bombTex = LoadTexture("picture/bomb.png");

    Player p1 = { { TILE_SIZE, 0.0f, TILE_SIZE }, BLUE, PLAYER1, 0, 0 };
    Player p2 = { { (GRID_SIZE - 2) * TILE_SIZE, 0.0f, (GRID_SIZE - 2) * TILE_SIZE }, RED, PLAYER2, 0, 0 };

    // ใส่ที่อยู่ของ Player เข้าไปใน Global Vector
    playerPtrs.push_back(&p1);
    playerPtrs.push_back(&p2);

    Camera camera = { 0 };
    float mapMid = ((GRID_SIZE - 1) * TILE_SIZE) / 2.0f;
    camera.position = { mapMid, 50.0f, mapMid };
    camera.target = { mapMid, 0.0f, mapMid };
    camera.up = { 0.0f, 0.0f, -1.0f };
    camera.projection = CAMERA_ORTHOGRAPHIC;

    float remainingTime = GAME_TIME;

    while (!WindowShouldClose()) {
        float dt = GetFrameTime();
        if (remainingTime > 0) remainingTime -= dt;

        camera.fovy = (float)GRID_SIZE * TILE_SIZE * ((float)GetScreenHeight() / GetScreenWidth()) * 1.7f;

        auto UpdatePlayer = [&](Player& p, int up, int down, int left, int right, int place, int detonate) {
            if (p.stunTimer > 0) { p.stunTimer -= dt; return; }
            Vector3 move = { 0 };
            if (IsKeyDown(up)) move.z = -1; if (IsKeyDown(down)) move.z = 1;
            if (IsKeyDown(left)) move.x = -1; if (IsKeyDown(right)) move.x = 1;

            if (Vector3Length(move) > 0) {
                move = Vector3Scale(Vector3Normalize(move), TILE_SIZE * 0.07f);
                float radius = TILE_SIZE * 0.35f;
                Vector3 nX = p.position; nX.x += move.x;
                int gx1 = (int)((nX.x - radius + TILE_SIZE / 2) / TILE_SIZE);
                int gx2 = (int)((nX.x + radius + TILE_SIZE / 2) / TILE_SIZE);
                int gz = (int)((p.position.z + TILE_SIZE / 2) / TILE_SIZE);
                if (gx1 >= 0 && gx2 < GRID_SIZE && map[gx1][gz].type == EMPTY && map[gx2][gz].type == EMPTY) p.position.x = nX.x;

                Vector3 nZ = p.position; nZ.z += move.z;
                int gz1 = (int)((nZ.z - radius + TILE_SIZE / 2) / TILE_SIZE);
                int gz2 = (int)((nZ.z + radius + TILE_SIZE / 2) / TILE_SIZE);
                int gx = (int)((p.position.x + TILE_SIZE / 2) / TILE_SIZE);
                if (gz1 >= 0 && gz2 < GRID_SIZE && map[gx][gz1].type == EMPTY && map[gx][gz2].type == EMPTY) p.position.z = nZ.z;
            }

            int curGX = (int)((p.position.x + TILE_SIZE / 2) / TILE_SIZE);
            int curGZ = (int)((p.position.z + TILE_SIZE / 2) / TILE_SIZE);

            if (IsKeyPressed(place)) {
                bool exists = false;
                for (auto& b : bombs) if ((int)b.pos.x == curGX && (int)b.pos.y == curGZ) exists = true;
                if (!exists) bombs.push_back({ {(float)curGX, (float)curGZ}, p.id });
            }
            if (IsKeyPressed(detonate)) {
                for (auto it = bombs.begin(); it != bombs.end();) {
                    if (it->creator == p.id) {
                        ExecuteExplosion((int)it->pos.x, (int)it->pos.y, p.id);
                        it = bombs.erase(it);
                    }
                    else ++it;
                }
            }
            };

        if (remainingTime > 0) {
            UpdatePlayer(p1, KEY_W, KEY_S, KEY_A, KEY_D, KEY_Q, KEY_SPACE);
            UpdatePlayer(p2, KEY_UP, KEY_DOWN, KEY_LEFT, KEY_RIGHT, KEY_RIGHT_CONTROL, KEY_ENTER);
        }

        p1.score = 0; p2.score = 0;
        for (int x = 0; x < GRID_SIZE; x++)
            for (int z = 0; z < GRID_SIZE; z++) {
                if (map[x][z].owner == PLAYER1) p1.score++;
                else if (map[x][z].owner == PLAYER2) p2.score++;
            }

        BeginDrawing();
        ClearBackground(RAYWHITE);
        BeginMode3D(camera);

        for (int x = 0; x < GRID_SIZE; x++) {
            for (int z = 0; z < GRID_SIZE; z++) {
                Vector3 pos = { x * TILE_SIZE, 0, z * TILE_SIZE };
                Color floorCol = (map[x][z].owner == PLAYER1) ? SKYBLUE : (map[x][z].owner == PLAYER2) ? PINK : LIGHTGRAY;
                DrawCube(pos, TILE_SIZE, 0.1f, TILE_SIZE, floorCol);
                DrawCubeWires({ pos.x, 0.1f, pos.z }, TILE_SIZE, 0.1f, TILE_SIZE, DARKGRAY);

                if (map[x][z].type == WALL_STEEL) {
                    DrawCube({ pos.x, 0.5f, pos.z }, TILE_SIZE, 1.0f, TILE_SIZE, GRAY);
                    DrawPlaneTexture(metalTex, { pos.x, 1.01f, pos.z }, TILE_SIZE, TILE_SIZE, WHITE);
                }
                if (map[x][z].type == WALL_WOOD) {
                    DrawCube({ pos.x, 0.5f, pos.z }, TILE_SIZE, 1.0f, TILE_SIZE, BROWN);
                    DrawPlaneTexture(woodTex, { pos.x, 1.01f, pos.z }, TILE_SIZE, TILE_SIZE, WHITE);
                }
            }
        }

        float bS = TILE_SIZE * 0.4f;
        for (auto& b : bombs) DrawPlaneTexture(bombTex, { b.pos.x * TILE_SIZE, 0.15f, b.pos.y * TILE_SIZE }, bS * 2, bS * 2, WHITE);

        float pY = 0.2f; float pS = TILE_SIZE * 0.47f;
        rlPushMatrix(); rlTranslatef(p1.position.x, pY, p1.position.z); rlRotatef(90, 1, 0, 0); DrawTexturePro(p1Tex, { 0, 0, (float)p1Tex.width, (float)p1Tex.height }, { -pS, -pS, pS * 2, pS * 2 }, { 0, 0 }, 0, p1.stunTimer > 0 ? YELLOW : WHITE); rlPopMatrix();
        rlPushMatrix(); rlTranslatef(p2.position.x, pY, p2.position.z); rlRotatef(90, 1, 0, 0); DrawTexturePro(p2Tex, { 0, 0, (float)p2Tex.width, (float)p2Tex.height }, { -pS, -pS, pS * 2, pS * 2 }, { 0, 0 }, 0, p2.stunTimer > 0 ? YELLOW : WHITE); rlPopMatrix();

        EndMode3D();

        DrawText(TextFormat("TIME: %.0f", (remainingTime > 0 ? remainingTime : 0)), GetScreenWidth() / 2 - 60, 20, 30, BLACK);
        DrawText(TextFormat("P1: %d", p1.score), 40, 20, 30, BLUE);
        DrawText(TextFormat("P2: %d", p2.score), GetScreenWidth() - 150, 20, 30, RED);

        if (remainingTime <= 0) {
            const char* winner = (p1.score > p2.score) ? "P1 WINS!" : (p2.score > p1.score) ? "P2 WINS!" : "DRAW!";
            DrawText(winner, GetScreenWidth() / 2 - MeasureText(winner, 60) / 2, GetScreenHeight() / 2, 60, MAROON);
        }
        EndDrawing();
    }

    UnloadTexture(p1Tex); UnloadTexture(p2Tex);
    UnloadTexture(metalTex); UnloadTexture(woodTex); UnloadTexture(bombTex);
    CloseWindow();
    return 0;
}