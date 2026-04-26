#include "raylib.h"
#include "raymath.h"
#include <vector>

const int GRID_SIZE = 15;
const float TILE_SIZE = 2.0f;
const float GAME_TIME = 180.0f;

enum TileType { EMPTY, WALL_STEEL, WALL_WOOD };
enum Owner { NONE, PLAYER1, PLAYER2 };

struct Tile {
    TileType type;
    Owner owner;
};

struct Bomb {
    Vector2 pos;
    Owner creator;
};

struct Player {
    Vector3 position;
    Color color;
    Owner id;
    float stunTimer;
    int score;
};

Tile map[GRID_SIZE][GRID_SIZE];
std::vector<Bomb> bombs;

void InitMap() {
    for (int x = 0; x < GRID_SIZE; x++) {
        for (int z = 0; z < GRID_SIZE; z++) {
            map[x][z].owner = NONE;
            if (x == 0 || x == GRID_SIZE - 1 || z == 0 || z == GRID_SIZE - 1 || (x % 2 == 0 && z % 2 == 0)) {
                map[x][z].type = WALL_STEEL;
            }
            else if (GetRandomValue(0, 10) > 4) {
                map[x][z].type = WALL_WOOD;
            }
            else {
                map[x][z].type = EMPTY;
            }
        }
    }
    // Clear area for players
    map[1][1].type = map[1][2].type = map[2][1].type = EMPTY;
    map[GRID_SIZE - 2][GRID_SIZE - 2].type = map[GRID_SIZE - 2][GRID_SIZE - 3].type = map[GRID_SIZE - 3][GRID_SIZE - 2].type = EMPTY;
}

void ExecuteExplosion(int bx, int by, Owner owner, std::vector<Player*>& players) {
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

            for (auto& p : players) {
                int px = (int)((p->position.x + TILE_SIZE / 2) / TILE_SIZE);
                int pz = (int)((p->position.z + TILE_SIZE / 2) / TILE_SIZE);
                if (px == nx && pz == nz) {
                    if (p->id != owner) p->stunTimer = 2.0f;
                }
            }
            if (map[nx][nz].type == WALL_WOOD) {
                map[nx][nz].type = EMPTY;
                break;
            }
        }
    }
}

int main() {
    InitWindow(1280, 720, "Remote Paint Bomber - 3D Grid");
    SetTargetFPS(60);
    InitMap();

    // Start players EXACTLY at the center of tile [1][1] and [13][13]
    Player p1 = { { 2.0f, 0.5f, 2.0f }, BLUE, PLAYER1, 0, 0 };
    Player p2 = { { (GRID_SIZE - 2) * TILE_SIZE, 0.5f, (GRID_SIZE - 2) * TILE_SIZE }, RED, PLAYER2, 0, 0 };
    std::vector<Player*> playerPtrs = { &p1, &p2 };

    Camera camera = { 0 };
    camera.position = { 14.0f, 60.0f, 14.0f }; // Centered
    camera.target = { 14.0f, 0.0f, 14.0f };
    camera.up = { 0.0f, 0.0f, -1.0f };
    camera.projection = CAMERA_ORTHOGRAPHIC;
    camera.fovy = 32.0f;

    float remainingTime = GAME_TIME;

    while (!WindowShouldClose()) {
        float dt = GetFrameTime();
        if (remainingTime > 0) remainingTime -= dt;

        auto UpdatePlayer = [&](Player& p, int up, int down, int left, int right, int place, int detonate) {
            if (p.stunTimer > 0) { p.stunTimer -= dt; return; }

            Vector3 move = { 0 };
            if (IsKeyDown(up)) move.z = -1;
            if (IsKeyDown(down)) move.z = 1;
            if (IsKeyDown(left)) move.x = -1;
            if (IsKeyDown(right)) move.x = 1;

            if (Vector3Length(move) > 0) {
                move = Vector3Scale(Vector3Normalize(move), 0.15f);
                float radius = 0.75f; // Balanced radius to prevent sticking

                // X Collision
                Vector3 nextPosX = p.position;
                nextPosX.x += move.x;
                int gx1 = (int)((nextPosX.x - radius + TILE_SIZE / 2) / TILE_SIZE);
                int gx2 = (int)((nextPosX.x + radius + TILE_SIZE / 2) / TILE_SIZE);
                int gz = (int)((p.position.z + TILE_SIZE / 2) / TILE_SIZE);

                if (gx1 >= 0 && gx2 < GRID_SIZE && map[gx1][gz].type == EMPTY && map[gx2][gz].type == EMPTY) {
                    p.position.x = nextPosX.x;
                }

                // Z Collision
                Vector3 nextPosZ = p.position;
                nextPosZ.z += move.z;
                int gz1 = (int)((nextPosZ.z - radius + TILE_SIZE / 2) / TILE_SIZE);
                int gz2 = (int)((nextPosZ.z + radius + TILE_SIZE / 2) / TILE_SIZE);
                int gx = (int)((p.position.x + TILE_SIZE / 2) / TILE_SIZE);

                if (gz1 >= 0 && gz2 < GRID_SIZE && map[gx][gz1].type == EMPTY && map[gx][gz2].type == EMPTY) {
                    p.position.z = nextPosZ.z;
                }
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
                        ExecuteExplosion((int)it->pos.x, (int)it->pos.y, p.id, playerPtrs);
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
                DrawCubeWires(pos, TILE_SIZE, 0.1f, TILE_SIZE, DARKGRAY);
                if (map[x][z].type == WALL_STEEL) DrawCube({ pos.x, 1.0f, pos.z }, TILE_SIZE, 2.0f, TILE_SIZE, GRAY);
                if (map[x][z].type == WALL_WOOD) DrawCube({ pos.x, 1.0f, pos.z }, TILE_SIZE, 2.0f, TILE_SIZE, BROWN);
            }
        }
        for (auto& b : bombs) DrawSphere({ b.pos.x * TILE_SIZE, 0.5f, b.pos.y * TILE_SIZE }, 0.6f, BLACK);
        DrawSphere(p1.position, 0.7f, p1.stunTimer > 0 ? YELLOW : p1.color);
        DrawSphere(p2.position, 0.7f, p2.stunTimer > 0 ? YELLOW : p2.color);
        EndMode3D();

        DrawText(TextFormat("TIME: %.0f", remainingTime > 0 ? remainingTime : 0), 580, 20, 30, BLACK);
        DrawText(TextFormat("P1 (BLUE): %d", p1.score), 20, 20, 20, BLUE);
        DrawText(TextFormat("P2 (RED): %d", p2.score), 1050, 20, 20, RED);
        if (remainingTime <= 0) {
            const char* winner = (p1.score > p2.score) ? "P1 WINS!" : (p2.score > p1.score) ? "P2 WINS!" : "DRAW!";
            DrawText(winner, 450, 300, 60, MAROON);
        }
        EndDrawing();
    }
    CloseWindow();
    return 0;
}