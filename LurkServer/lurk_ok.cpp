#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <netdb.h>
#include <map>
#include <string>
#include <vector>
#include <thread>
#include <mutex>
#include <algorithm>
#include <signal.h>
#include <unistd.h>

using namespace std;

// Forward declarations
struct player_state;
struct room;
struct character;

// Global state
map<string, player_state*> all_players;
map<uint16_t, vector<string>> room_occupants;
mutex players_mutex;
mutex rooms_mutex;
mutex bandit_mutex;
mutex seamonster_mutex;
mutex nightwitch_mutex;
mutex ghost_mutex;
mutex zombie_mutex;
mutex dwarf1_mutex;
mutex dwarf2_mutex;
mutex dwarf3_mutex;
mutex dwarf4_mutex;
mutex dwarf5_mutex;

// -------------------- error_msg --------------------
struct error_msg {
    uint8_t type = 7;
    uint8_t error_code;
    uint16_t error_msg_length;
    const char* description;
    const char* msg_list[10] = {
        "Other",
        "Bad room. Attempted to change to an inappropriate room",
        "Player Exists. Attempted to create a player that already exists.",
        "Bad Monster. Attempted to loot a nonexistent or not present monster or already looted.",
        "Total stats exceed the maximum allowed value.",    
        "Not Ready for this action yet.",
        "Invalid target. ",
        "No fight.",
        "No player vs. player combat on the server.",
        "Bruh you're dead already let your corpse rest."
    };

    void send_emsg(int ecode, int socket) {
        error_code = ecode;
        description = msg_list[ecode];
        error_msg_length = strlen(description);
        uint8_t header[4];
        header[0] = type;
        header[1] = error_code;
        header[2] = error_msg_length & 0xff;
        header[3] = (error_msg_length >> 8) & 0xff;
        write(socket, header, 4);
        write(socket, description, error_msg_length);
    }
};

// -------------------- accepted --------------------
struct accepted {
    uint8_t type = 8;
    void send_accepted(int socket, uint8_t code) {
        uint8_t accept_msg[2] = {type, code};
        write(socket, accept_msg, 2);
    }
};

// -------------------- room --------------------
struct room {
    uint8_t type = 9;
    uint16_t number;
    char name[32];
    uint16_t description_length;
    char* description;
    vector<room*> connections;

    room() { description = nullptr; }
    room(int num, const char* nom, char* desc) {
        number = num;
        strncpy(name, nom, 32);
        name[31] = '\0';
        description_length = strlen(desc);
        description = (char*)malloc(description_length + 1);
        memcpy(description, desc, description_length);
        description[description_length] = '\0';
    }

    void send_room(int socket) {
        unsigned char header[37];
        header[0] = type;
        header[1] = number & 0xff;
        header[2] = (number >> 8) & 0xff;
        memcpy(&header[3], name, 32);
        header[35] = description_length & 0xff;
        header[36] = (description_length >> 8) & 0xff;
        write(socket, header, sizeof(header));
        if (description_length > 0 && description) {
            write(socket, description, description_length);
        }
    }

    void send_connections(int socket) {
        for (room* r : connections) {
            uint8_t type = 13;
            uint8_t header[37];
            header[0] = type;
            header[1] = r->number & 0xff;
            header[2] = (r->number >> 8) & 0xff;
            memcpy(&header[3], r->name, 32);
            header[35] = r->description_length & 0xff;
            header[36] = (r->description_length >> 8) & 0xff;
            write(socket, header, 37);
            if (r->description_length > 0) {
                write(socket, r->description, r->description_length);
            }
        }
    }
};

// -------------------- character --------------------
struct character {
    uint8_t type = 10;
    char name[32];
    uint8_t flags;
    uint16_t attack;
    uint16_t defense;
    uint16_t regen;
    int16_t health;
    uint16_t gold;
    uint16_t current_room;
    uint16_t description_length;
    char* description = 0;

    character() {
        memset(name, 0, 32);
        flags = 0;
        attack = defense = regen = gold = description_length = 0;
        health = 100;
        current_room = 0;
    }

    bool receive_character(int socket) {
        uint8_t header[48];
        int received = recv(socket, header, sizeof(header), MSG_WAITALL);
        if (received <= 0) {
            printf("Character receive failed: expected 48 bytes, got %d\n", received);
            return false;
        }
        // Zero out header if not fully received
        if (received < 48) {
            memset(header + received, 0, 48 - received);
        }
        memcpy(name, &header[1], 32);
        flags = header[33];
        attack = header[34] | (header[35] << 8);
        defense = header[36] | (header[37] << 8);
        regen = header[38] | (header[39] << 8);
        health = (int16_t)(header[40] | (header[41] << 8));
        gold = header[42] | (header[43] << 8);
        current_room = header[44] | (header[45] << 8);
        description_length = header[46] | (header[47] << 8);

        printf("Received character: name=%s, attack=%d, defense=%d, regen=%d, health=%d, desc_len=%d\n", 
               name, attack, defense, regen, health, description_length);

        if (description != nullptr) {
            free(description);
            description = nullptr;
        }
        if (description_length > 0) {
            description = (char*)malloc(description_length + 1);
            int received2 = recv(socket, description, description_length, MSG_WAITALL);
            if (received2 != description_length) {
                printf("Failed to receive description: expected %d bytes, got %d\n", description_length, received2);
                return false;
            }
            description[description_length] = '\0';
        }
        return true;
    }

    void set_started() {
        flags = 0x80 | 0x10 | 0x08; // Alive | Started | Ready
    }
    void check_death() {
        if (health <= 0) {
            flags &= ~0x80; // Clear the "Alive" bit (bit 7)
            health = 0; // Ensure health doesn't go negative
        }
    }

    void send_character(int socket) {
        uint8_t header[48];
        header[0] = type;
        memcpy(&header[1], name, 32);
        header[33] = flags;
        header[34] = attack & 0xff;
        header[35] = (attack >> 8) & 0xff;
        header[36] = defense & 0xff;
        header[37] = (defense >> 8) & 0xff;
        header[38] = regen & 0xff;
        header[39] = (regen >> 8) & 0xff;
        header[40] = health & 0xff;
        header[41] = (health >> 8) & 0xff;
        header[42] = gold & 0xff;
        header[43] = (gold >> 8) & 0xff;
        header[44] = current_room & 0xff;
        header[45] = (current_room >> 8) & 0xff;
        header[46] = description_length & 0xff;
        header[47] = (description_length >> 8) & 0xff;
        write(socket, header, 48);
        if (description_length > 0 && description != nullptr) {
            write(socket, description, description_length);
        }
    }

    void character_fight(character& opponent, int fd) {
        bool is_bandit = strcmp(opponent.name, "BANDIT") == 0;
        bool is_seamonster = strcmp(opponent.name, "SEA MONSTER") == 0;
        bool is_nightwitch = strcmp(opponent.name, "NIGHT WITCH") == 0;
        bool is_ghost = strcmp(opponent.name, "GHOST") == 0;
        bool is_zombie = strcmp(opponent.name, "ZOMBIE") == 0;
        bool is_dwarf1 = strcmp(opponent.name, "DWARF1") == 0;
        bool is_dwarf2 = strcmp(opponent.name, "DWARF2") == 0;
        bool is_dwarf3 = strcmp(opponent.name, "DWARF3") == 0;
        bool is_dwarf4 = strcmp(opponent.name, "DWARF4") == 0;
        bool is_dwarf5 = strcmp(opponent.name, "DWARF5") == 0;
        // Simple fight logic: each character deals damage equal to their attack minus opponent's defense
        int damage_to_opponent = max(0, this->attack - opponent.defense);
        int damage_to_self = max(0, opponent.attack - this->defense);

        if(!is_bandit && !is_seamonster && !is_nightwitch && !is_ghost && !is_zombie && !is_dwarf1 && !is_dwarf2 && !is_dwarf3 && !is_dwarf4 && !is_dwarf5){
            opponent.health -= damage_to_opponent>0 ? damage_to_opponent : 0;
        }else{
        opponent.health -= damage_to_opponent>0 ? damage_to_opponent : 0;
        this->health -= damage_to_self >0 ? damage_to_self : 0;
        }
    
        this->check_death();
        opponent.check_death();
    }
    


};

// -------------------- version --------------------
struct version {
    uint8_t type = 14;
    uint8_t majorv;
    uint8_t minorv;
    uint16_t extension_len = 0;

    version(uint8_t maj, uint8_t min) : majorv(maj), minorv(min) {}

    void send_version(int socket) {
        uint8_t data[5];
        data[0] = type;
        data[1] = majorv;
        data[2] = minorv;
        data[3] = extension_len & 0xff;
        data[4] = (extension_len >> 8) & 0xff;
        write(socket, data, 5);
    }
};

// -------------------- game --------------------
struct game {
    uint8_t type = 11;
    uint16_t initial_pts;
    uint16_t stat_limit;
    uint16_t description_len;
    char* description;

    game(uint16_t ip, uint16_t sl, char* d) : initial_pts(ip), stat_limit(sl), description(d) {}

    void send_game(int socket) {
        description_len = strlen(description);
        uint8_t header[7];
        header[0] = type;
        header[1] = initial_pts & 0xff;
        header[2] = (initial_pts >> 8) & 0xff;
        header[3] = stat_limit & 0xff;
        header[4] = (stat_limit >> 8) & 0xff;
        header[5] = description_len & 0xff;
        header[6] = (description_len >> 8) & 0xff;
        write(socket, header, 7);
        write(socket, description, description_len);
    }
};

// -------------------- Global Objects --------------------
room souk, alley, shop, house, gate, harbor, farm, desert, oasis, cave, ship, fort;
accepted acc;
error_msg err;
game* global_game = nullptr;

// Monster definitions
character bandit;
character sea_monster;
character night_witch;
character ghost;
character zombie;
character dwarf1;
character dwarf2;
character dwarf3;
character dwarf4;
character dwarf5;

void setup_dwarf(string name, uint8_t room_num, character& dwarf) {
    strcpy(dwarf.name, name.c_str());
    dwarf.flags = 0x80 | 0x20; // Alive | Monster
    dwarf.attack = 33;
    dwarf.defense = 45;
    dwarf.regen = 5;
    dwarf.health = 70;
    dwarf.gold = 467; // Loot
    dwarf.current_room = room_num; 
    const char* desc = "dwarf is not very strong but he is very tough and hard to kill. He has a lot of gold but he is not willing to part with it easily.";
    dwarf.description_length = strlen(desc);
    dwarf.description = (char*)malloc(dwarf.description_length + 1);
    strcpy(dwarf.description, desc);

}

void setup_monsters() {
    // BANDIT in Narrow Alley
    strcpy(bandit.name, "BANDIT");
    bandit.flags = 0x80 | 0x20; // Alive | Monster
    bandit.attack = 45;
    bandit.defense = 10;
    bandit.regen = 0;
    bandit.health = 40;
    bandit.gold = 373;
    bandit.current_room = 2; // Narrow Alley
    const char* desc = "A sneaky bandit lurks in the shadows, ready to steal your gold. He looks dangerous and is known to ambush travelers. Beware: he will attack you if you enter!";
    bandit.description_length = strlen(desc);
    bandit.description = (char*)malloc(bandit.description_length + 1);
    strcpy(bandit.description, desc);

    // Sea Monster in Harbor
    strcpy(sea_monster.name, "SEA MONSTER");
    sea_monster.flags = 0x80 | 0x20; // Alive | Monster
    sea_monster.attack = 75;
    sea_monster.defense = 35;
    sea_monster.regen = 10;
    sea_monster.health = 601;
    sea_monster.gold = 2500; // Reward from sailors
    sea_monster.current_room = 6; // Harbor
    const char* desc2 = "A massive sea monster rises from the water, its tentacles thrashing. It is fighting a group of terrified sailors. You can try to save them by fighting the monster!";
    sea_monster.description_length = strlen(desc2);
    sea_monster.description = (char*)malloc(sea_monster.description_length + 1);
    strcpy(sea_monster.description, desc2);

    // Night Witch in Abandoned House
    strcpy(night_witch.name, "NIGHT WITCH");
    night_witch.flags = 0x80 | 0x20; // Alive | Monster
    night_witch.attack = 50;
    night_witch.defense = 25;
    night_witch.regen = 20;
    night_witch.health = 47;
    night_witch.gold = 273; // Loot
    night_witch.current_room = 4; // Abandoned House
    const char* desc3 = "The Night Witch, a sinister figure cloaked in darkness, stands before you. Her eyes gleam with malevolent intent as she prepares to cast a deadly spell. Defeating her could yield valuable treasures.";
    night_witch.description_length = strlen(desc3);
    night_witch.description = (char*)malloc(night_witch.description_length + 1);
    strcpy(night_witch.description, desc3);

    //ghost in cave
    strcpy(ghost.name, "GHOST");
    ghost.flags = 0x80 | 0x20; // Alive | Monster
    ghost.attack = 55;
    ghost.defense = 15;
    ghost.regen = 5;
    ghost.health = 40;
    ghost.gold = 340; // Loot
    ghost.current_room = 10; // Cave
    const char* desc4 = "A translucent ghost hovers in the cave, its mournful wails echoing off the walls. It seems bound to this place, perhaps guarding hidden secrets. Confronting it may reveal valuable treasures.";
    ghost.description_length = strlen(desc4);
    ghost.description = (char*)malloc(ghost.description_length + 1);
    strcpy(ghost.description, desc4);

    //zombie in desert
    strcpy(zombie.name, "ZOMBIE");
    zombie.flags = 0x80 | 0x20; // Alive | Monster
    zombie.attack = 40;
    zombie.defense = 20;
    zombie.regen = 0;
    zombie.health = 60;
    zombie.gold = 220; // Loot
    zombie.current_room = 8; // Desert
    const char* desc5 = "A decaying zombie shambles through the desert, its vacant eyes searching for prey. It moves with a relentless hunger, driven by an insatiable desire. Defeating it could yield valuable treasures.";
    zombie.description_length = strlen(desc5);
    zombie.description = (char*)malloc(zombie.description_length + 1);
    strcpy(zombie.description, desc5);

    //dwarf in farm, old shop, souk, house, harbor, gate, alley
    setup_dwarf("DWARF1", 7, dwarf1);//farm
    setup_dwarf("DWARF2", 3, dwarf2);//old shop
    setup_dwarf("DWARF3", 1, dwarf3);//souk
    setup_dwarf("DWARF4", 11, dwarf4);//ship
    setup_dwarf("DWARF5", 12, dwarf5);//fort

    rooms_mutex.lock();
    room_occupants[1].push_back("DWARF3");
    rooms_mutex.unlock();

}


void setup_rooms() {
    souk = room(1, "Souk", (char*)"You're in a Souk Market, surrounded by colorful fabrics, glinting lanterns, and piles of spices. Merchants call out, scents of cinnamon and saffron fill the air, and every corner hides a small treasure.");
    gate = room(5, "City Gate", (char*)"You stand before Bab el Medina,  the grand gate of the old city. The worn stones bear marks of centuries of travelers, soldiers, and merchants passing through. Outside, the coastal breeze carries distant gull cries.");
    harbor = room(6, "Harbor", (char*)"The harbor bustles with life, ships unloading barrels of olives and spices, sailors arguing over wages, and children darting between nets hung to dry. The sea glimmers in the sun beyond the fort's shadow.");
    alley = room(2, "Narrow Alley", (char*)"A cramped alley branches off to the right, where shadows cling to the walls. The smell of smoke and roasting meat drifts from a hidden grill, while faint whispers echo from deeper within.");
    shop = room(3, "Old Shop", (char*)"An old, dimly lit shop leans against the main market street. Dust coats the shelves of trinkets, and the merchant avoids your gaze with a crooked smile. Something about the place feels off, especially the door in the back, half-hidden by a curtain.");
    house = room(4, "Abandoned House", (char*)"Through the backdoor, you step into a crumbling house. The wooden beams creak overhead, and faded carpets cover the stone floor. It feels like someone left in a hurry, yet a lantern still flickers weakly in the corner.");
    
    farm = room(7, "Farm", (char*)"You arrive at a peaceful farm on the outskirts of the city. Rows of vegetables grow in neat lines, and a farmer tends to his crops. The scent of fresh earth and blooming flowers fills the air.");
    desert = room(8, "Desert", (char*)"You find yourself in a vast desert, with rolling dunes stretching as far as the eye can see. The sun blazes overhead, and the heat shimmers off the sand. In the distance, you spot an oasis with palm trees and a small pool of water.");
    oasis = room(9, "Oasis", (char*)"You step into a lush oasis, a stark contrast to the surrounding desert. Palm trees sway gently in the breeze, and a clear pool of water reflects the blue sky. Birds chirp in the trees, and the air is filled with the scent of blooming flowers.");
    cave = room(10, "Cave", (char*)"You enter a dark cave, the air cool and damp. Stalactites hang from the ceiling, and the sound of dripping water echoes through the cavern. Faint glimmers of light reflect off mineral deposits in the rock walls.");
    fort = room(12, "Fort", (char*)"You stand within the walls of an ancient fort overlooking the harbor. The stone battlements are weathered but sturdy, and the view of the sea is breathtaking. Cannons line the walls, remnants of past battles defending the city.");
    ship = room(11, "Ship", (char*)"You board an old ship docked at the harbor. The wooden deck creaks underfoot, and the salty sea air fills your lungs. Ropes and sails are neatly coiled, and the captain's quarters are just ahead.");

    souk.connections = {&alley, &shop, &farm, &desert};
    alley.connections = {&souk, &gate};
    gate.connections = {&alley, &harbor};
    harbor.connections = {&gate, &fort};
    fort.connections = {&harbor, &ship};
    ship.connections = {&fort};
    shop.connections = {&souk, &house};
    house.connections = {&shop};
    farm.connections = {&souk};
    desert.connections = {&souk, &oasis};
    oasis.connections = {&desert, &cave};
    cave.connections = {&oasis};

    // After setting up rooms and connections
    setup_monsters();
}

// -------------------- player_state --------------------
struct player_state {
    character player_char;
    int socket_fd;
    mutex send_mutex;
    bool active;
    bool bandit_loot_eligible = false;
    bool seamonster_loot_eligible = false;
    bool nightwitch_loot_eligible = false;
    bool ghost_loot_eligible = false;
    bool zombie_loot_eligible = false;
    bool dwarf1_loot_eligible = false;
    bool dwarf2_loot_eligible = false;
    bool dwarf3_loot_eligible = false;
    bool dwarf4_loot_eligible = false;
    bool dwarf5_loot_eligible = false;


};

// -------------------- Helper Functions --------------------

void send_narration(int fd, const char* message) {
    // Send MESSAGE type 1 with narration marker
    uint8_t header[67];
    header[0] = 1; // MESSAGE type
    uint16_t msg_len = strlen(message);
    header[1] = msg_len & 0xff;
    header[2] = (msg_len >> 8) & 0xff;
    memset(&header[3], 0, 32); // recipient (empty for narration)
    memset(&header[35], 0, 32); // sender (empty for narration)
    header[65] = 0; // narration marker byte 1
    header[66] = 1; // narration marker byte 2
    write(fd, header, 67);
    write(fd, message, msg_len);
}

void broadcast_character_to_room(uint16_t room_num, character& ch, string exclude = "") {
    rooms_mutex.lock();
    vector<string> occupants = room_occupants[room_num];
    rooms_mutex.unlock();
    
    for (const string& name : occupants) {
        if (name == exclude) continue;
        
        players_mutex.lock();
        auto it = all_players.find(name);
        if (it == all_players.end()) {
            players_mutex.unlock();
            continue;
        }
        player_state* ps = it->second;
        
        // CHANGED: Lock send_mutex BEFORE releasing players_mutex
        ps->send_mutex.lock();
        players_mutex.unlock();
        
        // ADDED: Double-check player is still active
        if (ps->active) {
            ch.send_character(ps->socket_fd);
        }
        ps->send_mutex.unlock();
    }
}
void send_room_entry(int socket, room& r, string player_name, player_state* ps) {
    // 1. Send ROOM
    r.send_room(socket);
    
    // 2. Send updated CHARACTER for this player (room already updated in handle_room_change)
    ps->player_char.send_character(socket);
    
    // 3. Send CONNECTIONs
    r.send_connections(socket);
    
    // 4. Send CHARACTERs for other players and monsters in room (not the player himself)
    rooms_mutex.lock();
    vector<string> occupants = room_occupants[r.number];
    rooms_mutex.unlock();
    for (const string& name : occupants) {
        if (name == player_name) continue;
        // Check if it's a monster
        if (name == "BANDIT") {
            bandit.send_character(socket);
        } else if (name == "SEA MONSTER") {
            sea_monster.send_character(socket);
        } else if (name == "NIGHT WITCH") {
            night_witch.send_character(socket);
        }else if (name == "GHOST") {
            ghost.send_character(socket);
        } else if (name == "ZOMBIE") {
            zombie.send_character(socket);
        } else if (name == "DWARF1") {
            dwarf1.send_character(socket);
        } else if (name == "DWARF2") {
            dwarf2.send_character(socket);
        } else if (name == "DWARF3") {
            dwarf3.send_character(socket);
        } else if (name == "DWARF4") {
            dwarf4.send_character(socket);
        } else if (name == "DWARF5") {
            dwarf5.send_character(socket);
        }
        else {
            players_mutex.lock();
            auto it = all_players.find(name);
            if (it != all_players.end()) {
                player_state* other = it->second;
                players_mutex.unlock();
                other->player_char.send_character(socket);
            } else {
                players_mutex.unlock();
            }
        }
    }
}



void handle_message_relay(int fd, string sender) {
    
    // MESSAGE: msg_len(2) + recipient(32) + sender(32) = 66 bytes (type already consumed)
    uint8_t header[66];
    int n = recv(fd, header, 66, MSG_WAITALL);
    if(n != 66) {
        return;
    }
    
    uint16_t msg_len = header[0] | (header[1] << 8);

    char recipient_name[33];
    memcpy(recipient_name, &header[2], 32);
    recipient_name[32] = '\0';
    
    // Skip if msg_len is 0 (empty message)
    if(msg_len == 0) {
        return;
    }
    
    char* message_text = (char*)malloc(msg_len);
    n = recv(fd, message_text, msg_len, MSG_WAITALL);
    if(n != msg_len) {
        free(message_text);
        return;
    }
    
    players_mutex.lock();
    auto it = all_players.find(string(recipient_name));
    if (it == all_players.end()) {
        players_mutex.unlock();
        err.send_emsg(6, fd);
        free(message_text);
        return;
    }
    player_state* recipient = it->second;
    players_mutex.unlock();
    
    recipient->send_mutex.lock();
    uint8_t type = 1;
    write(recipient->socket_fd, &type, 1);
    write(recipient->socket_fd, header, 66);
    write(recipient->socket_fd, message_text, msg_len);
    recipient->send_mutex.unlock();
    
    free(message_text);
    acc.send_accepted(fd, 1);
}

room* get_room(uint16_t room_num) {
    if(room_num == 1) return &souk;
    if(room_num == 2) return &alley;
    if(room_num == 3) return &shop;
    if(room_num == 4) return &house;
    if(room_num == 5) return &gate;   
    if(room_num == 6) return &harbor;
    if(room_num == 7) return &farm;
    if(room_num == 8) return &desert;
    if(room_num == 9) return &oasis;
    if(room_num == 10) return &cave;
    if(room_num == 11) return &ship;
    if(room_num == 12) return &fort;
    return nullptr;
}

bool is_connected(uint16_t from_room, uint16_t to_room) {
    room* r = get_room(from_room);
    if (!r) return false;
    for (room* conn : r->connections) {
        if (conn->number == to_room) return true;
    }
    return false;
}



void handle_room_change(int fd, string player_name) {
    uint8_t data[2];
    if(recv(fd, data, 2, MSG_WAITALL) != 2) {
        printf("Failed to read room change data for %s\n", player_name.c_str());
        return;
    }
    uint16_t new_room_num = data[0] | (data[1] << 8);
    
    players_mutex.lock();
    auto it = all_players.find(player_name);
    if (it == all_players.end()) {
        players_mutex.unlock();
        printf("Player %s not found in all_players\n", player_name.c_str());
        return;
    }
    player_state* ps = it->second;
    uint16_t old_room = ps->player_char.current_room;
    players_mutex.unlock();
    
    printf("%s attempting to move from room %d to room %d\n", player_name.c_str(), old_room, new_room_num);
    
    // Validate connection
    if (!is_connected(old_room, new_room_num)) {
        printf("Room %d not connected to room %d, sending error\n", old_room, new_room_num);
        err.send_emsg(1, fd);
        return;
    }
    
    room* new_room = get_room(new_room_num);
    if (!new_room) {
        printf("Room %d not found, sending error\n", new_room_num);
        err.send_emsg(1, fd);
        return;
    }
    
    printf("%s successfully moving from room %d to room %d\n", player_name.c_str(), old_room, new_room_num);

    // UPDATE THE PLAYER'S ROOM NUMBER
    ps->player_char.current_room = new_room_num;

    // Remove from old room
    rooms_mutex.lock();
    auto& old_occupants = room_occupants[old_room];
    old_occupants.erase(remove(old_occupants.begin(), old_occupants.end(), player_name), old_occupants.end());
    rooms_mutex.unlock();

    // Add to new room
    rooms_mutex.lock();
    room_occupants[new_room_num].push_back(player_name);
    rooms_mutex.unlock();

    // Send room entry to this player
    send_room_entry(fd, *new_room, player_name, ps);

    // Broadcast to others in new room that this player entered
    broadcast_character_to_room(old_room, ps->player_char, player_name);

    // Check for monsters in the new room
    if (new_room_num == 2) { // Narrow Alley
        // Place BANDIT in room if not already present
        bool bandit_present = false;
        rooms_mutex.lock();
        for (const string& name : room_occupants[2]) {
            if (name == "BANDIT") bandit_present = true;
        }
        if (!bandit_present) {
            // Reset bandit to alive before spawning
            bandit.health = 57;
            bandit.gold = 373;
            bandit.flags = 0x80 | 0x20; //Alive and Monster bit
            room_occupants[2].push_back("BANDIT");
        }
        rooms_mutex.unlock();
        // Send BANDIT character to player
        bandit.send_character(fd);
        // Immediate fight with 3 descriptive messages
        // 1st message: Encounter
        send_narration(fd, "A bandit leaps from the shadows and attacks you!");
        // 2nd message: Fight description
        send_narration(fd, "The bandit swings his dagger, slashing at your side! You struggle to defend yourself.");
        // 3rd message: Outcome and update stats
        ps->player_char.health -= (bandit.attack - ps->player_char.defense) > 0 ? (bandit.attack - ps->player_char.defense) : 10; // Simple damage calculation
        ps->player_char.check_death();
        broadcast_character_to_room(new_room_num, ps->player_char, player_name);

        
        if (ps->player_char.health > 0) {
            send_narration(fd, "You manage to fend off the bandit, but you are wounded!");
        } else {
            send_narration(fd, "The bandit overpowers you and you collapse!");
        }
        // Send updated stats after fight (only one character update needed)
        ps->player_char.send_character(fd);
        bandit.send_character(fd);
    }else if (new_room_num == 4) { // Abandoned House
    // Place NIGHT WITCH in room if not already present
    bool witch_present = false;
    rooms_mutex.lock();
    for (const string& name : room_occupants[4]) {
        if (name == "NIGHT WITCH") witch_present = true;
    }
    if (!witch_present) {
        // Reset night witch to alive before spawning
        night_witch.health = 47;
        night_witch.gold = 273; // Reset gold in case looted before
        night_witch.flags = 0x80 | 0x20; // Alive + Monster bit
        room_occupants[4].push_back("NIGHT WITCH");
        
    }
    rooms_mutex.unlock();
    // Send NIGHT WITCH character to player
    night_witch.send_character(fd);
    // Do NOT auto-fight. Player must send FIGHT command to engage.
    } else if (new_room_num == 6) { // Harbor
        // Place SEA MONSTER in room if not already present
        bool monster_present = false;
        rooms_mutex.lock();
        for (const string& name : room_occupants[6]) {
            if (name == "SEA MONSTER") monster_present = true;
        }
        if (!monster_present) {
            // Reset night witch to alive before spawning
            sea_monster.health = 617;
            sea_monster.gold = 2503; // Reset gold in case looted before
            sea_monster.flags = 0x80 | 0x20; // Alive + Monster bit
            room_occupants[6].push_back("SEA MONSTER");
           
        }
        rooms_mutex.unlock();
        // Send SEA MONSTER character to player (description says you can choose to fight)
        sea_monster.send_character(fd);
        // Do NOT auto-fight. Player must send FIGHT command to engage.
    }else if (new_room_num ==10) { // Cave
    // Place GHOST in room if not already present
        bool ghost_present = false;
        rooms_mutex.lock();
        for (const string& name : room_occupants[10]) {
            if (name == "GHOST") ghost_present = true;
        }
        if (!ghost_present) {
            // Reset ghost to alive before spawning
            ghost.health = 40;
            ghost.gold = 340; // Reset gold in case looted before
            ghost.flags = 0x80 | 0x20; // Alive + Monster bit
            room_occupants[10].push_back("GHOST");
        }
        rooms_mutex.unlock();
        // Send GHOST character to player
        ghost.send_character(fd);
        // Do NOT auto-fight. Player must send FIGHT command to engage.
    }else if (new_room_num ==8) { // Desert
    // Place ZOMBIE in room if not already present
        bool zombie_present = false;
        rooms_mutex.lock();
        for (const string& name : room_occupants[8]) {
            if (name == "ZOMBIE") zombie_present = true;
        }
        if (!zombie_present) {
            // Reset zombie to alive before spawning
            zombie.health = 60;
            zombie.gold = 220; // Reset gold in case looted before
            zombie.flags = 0x80 | 0x20; // Alive + Monster bit
            room_occupants[8].push_back("ZOMBIE");
        }
        rooms_mutex.unlock();
        // Send ZOMBIE character to player
        zombie.send_character(fd);
        // Do NOT auto-fight. Player must send FIGHT command to engage.
    }else if(new_room_num ==7) { // Farm
        // Place DWARF1 in room if not already present
        bool dwarf1_present = false;
        rooms_mutex.lock();
        for (const string& name : room_occupants[7]) {
            if (name == "DWARF1") dwarf1_present = true;
        }
        if (!dwarf1_present) {
            // Reset dwarf1 to alive before spawning
            dwarf1.health = 70;
            dwarf1.gold = 467; // Reset gold in case looted before
            dwarf1.flags = 0x80 | 0x20; // Alive + Monster bit
            room_occupants[7].push_back("DWARF1");
        }
        rooms_mutex.unlock();
        // Send DWARF1 character to player
        dwarf1.send_character(fd);
        // Do NOT auto-fight. Player must send FIGHT command to engage.
    }else if(new_room_num ==3) { // Old Shop
        // Place DWARF2 in room if not already present
        bool dwarf2_present = false;
        rooms_mutex.lock();
        for (const string& name : room_occupants[3]) {
            if (name == "DWARF2") dwarf2_present = true;
        }
        if (!dwarf2_present) {
            // Reset dwarf2 to alive before spawning
            dwarf2.health = 70;
            dwarf2.gold = 467; // Reset gold in case looted before
            dwarf2.flags = 0x80 | 0x20; // Alive + Monster bit
            room_occupants[3].push_back("DWARF2");
        }
        rooms_mutex.unlock();
        // Send DWARF2 character to player
        dwarf2.send_character(fd);
        // Do NOT auto-fight. Player must send FIGHT command to engage.
    }else if(new_room_num ==1) { // Souk
        // Place DWARF3 in room if not already present
        bool dwarf3_present = false;
        rooms_mutex.lock();
        for (const string& name : room_occupants[1]) {
            if (name == "DWARF3") dwarf3_present = true;
        }
        if (!dwarf3_present) {
            // Reset dwarf3 to alive before spawning
            dwarf3.health = 70;
            dwarf3.gold = 467; // Reset gold in case looted before
            dwarf3.flags = 0x80 | 0x20; // Alive + Monster bit
            room_occupants[1].push_back("DWARF3");
        }
        rooms_mutex.unlock();
        // Send DWARF3 character to player
        dwarf3.send_character(fd);
        // Do NOT auto-fight. Player must send FIGHT command to engage.
    }else if(new_room_num ==11) { // Ship
        // Place DWARF4 in room if not already present
        bool dwarf4_present = false;
        rooms_mutex.lock();
        for (const string& name : room_occupants[11]) {
            if (name == "DWARF4") dwarf4_present = true;
        }
        if (!dwarf4_present) {
            // Reset dwarf4 to alive before spawning
            dwarf4.health = 70;
            dwarf4.gold = 467; // Reset gold in case looted before
            dwarf4.flags = 0x80 | 0x20; // Alive + Monster bit
            room_occupants[11].push_back("DWARF4");
        }
        rooms_mutex.unlock();
        // Send DWARF4 character to player
        dwarf4.send_character(fd);
        // Do NOT auto-fight. Player must send FIGHT command to engage.
    }else if(new_room_num ==12) { // Fort
        // Place DWARF5 in room if not already present
        bool dwarf5_present = false;
        rooms_mutex.lock();
        for (const string& name : room_occupants[12]) {
            if (name == "DWARF5") dwarf5_present = true;
        }
        if (!dwarf5_present) {
            // Reset dwarf5 to alive before spawning
            dwarf5.health = 70;
            dwarf5.gold = 467; // Reset gold in case looted before
            dwarf5.flags = 0x80 | 0x20; // Alive + Monster bit
            room_occupants[12].push_back("DWARF5");
        }
        rooms_mutex.unlock();
        // Send DWARF5 character to player
        dwarf5.send_character(fd);
        // Do NOT auto-fight. Player must send FIGHT command to engage.
    }else if (new_room_num == 9) {
        send_narration(fd, "You arrive at the oasis, feeling refreshed by the shade of the palm trees and the cool water.");
    }
    else if (new_room_num == 5) {
        if(ps->player_char.health >0 && ps->player_char.health <100){
            send_narration(fd, "you find the enchanted fountain and you drink from its magical waters. You feel rejuvenated as your health is fully restored!");
            ps->player_char.health = 100;
        }else if(ps->player_char.health <=0){
            ps->player_char.flags |= 0x80; // Revive the player
            ps->player_char.health = 100;
            send_narration(fd, "As you approach the enchanted fountain, you get revived by its magical waters!");
        }

        ps->player_char.send_character(fd);
        broadcast_character_to_room(new_room_num, ps->player_char, player_name);
    }



}

void cleanup_player(string name) {
    players_mutex.lock();
    auto it = all_players.find(name);
    if (it != all_players.end()) {
        uint16_t room_num = it->second->player_char.current_room;
        delete it->second;
        all_players.erase(it);
        players_mutex.unlock();
        
        // Remove from room
        rooms_mutex.lock();
        auto& occupants = room_occupants[room_num];
        for (auto it = occupants.begin(); it != occupants.end(); ) {
            if (*it == name) {
                it = occupants.erase(it);
            } else {
                ++it;
            }
        }
        rooms_mutex.unlock();
    } else {
        players_mutex.unlock();
    }
}

// -------------------- Fight and Loot Handlers --------------------




// Updated handle_fight function
void handle_fight(int fd, string player_name) {
    
    // Get player's current room
    players_mutex.lock();
    auto it = all_players.find(player_name);
    if (it == all_players.end()) {
        players_mutex.unlock();
        err.send_emsg(6, fd);
        return;
    }
    player_state* ps = it->second;
    if (!(ps->player_char.flags & 0x80)) {
        players_mutex.unlock();
        err.send_emsg(9, fd);
        return;
    }
    uint16_t room_num = ps->player_char.current_room;
    players_mutex.unlock();

    // Check for monster in room
    rooms_mutex.lock();
    auto& occupants = room_occupants[room_num];
    bool bandit_present = false, seamonster_present = false, nightwitch_present = false;
    bool ghost_present = false, zombie_present = false;
    bool dwarf1_present = false, dwarf2_present = false, dwarf3_present = false;
    bool dwarf4_present = false, dwarf5_present = false;
    for (const string& occ : occupants) {
        if (occ == "BANDIT") bandit_present = true;
        if (occ == "SEA MONSTER") seamonster_present = true;
        if (occ == "NIGHT WITCH") nightwitch_present = true;
        if (occ == "GHOST") ghost_present = true;
        if (occ == "ZOMBIE") zombie_present = true;
        if (occ == "DWARF1") dwarf1_present = true;
        if (occ == "DWARF2") dwarf2_present = true;
        if (occ == "DWARF3") dwarf3_present = true;
        if (occ == "DWARF4") dwarf4_present = true;
        if (occ == "DWARF5") dwarf5_present = true;
    }
    rooms_mutex.unlock();

    if (bandit_present) {
        send_narration(fd, "You attack the bandit with all your might! trying to take revenge for the ambush!");
        send_narration(fd, "The bandit lunges at you with his dagger!");

        bandit_mutex.lock();
        ps->player_char.character_fight(bandit, fd);
        bandit_mutex.unlock();
        // Send updated character status
        ps->player_char.send_character(fd);
        bandit.send_character(fd);

        broadcast_character_to_room(room_num, ps->player_char, player_name);
        broadcast_character_to_room(room_num, bandit, "");
        if (bandit.health <= 0) {
            send_narration(fd, "You have defeated the bandit! You can now loot his belongings.");
            // Remove bandit from room (defeated)
            rooms_mutex.lock();
            auto& occ = room_occupants[2];
            occ.erase(remove(occ.begin(), occ.end(), "BANDIT"), occ.end());
            rooms_mutex.unlock();

            // Make EVERYONE in the room eligible to loot
            rooms_mutex.lock();
            vector<string> room_players = room_occupants[room_num];
            rooms_mutex.unlock();
            
            players_mutex.lock();
            for (const string& name : room_players) {
                auto player_it = all_players.find(name);
                if (player_it != all_players.end()) {
                    player_it->second->bandit_loot_eligible = true;
                }
            }
            players_mutex.unlock();
        } else if (ps->player_char.health <= 0) {
            send_narration(fd, "The bandit overpowers you! You collapse onto the ground, defeated.");
        } else {
            send_narration(fd, "The fight continues! Both you and the bandit are wounded but still standing.");
        }
        return;
    } else if (seamonster_present) {
        // Fight the sea monster
        send_narration(fd, "You charge at the sea monster, drawing its attention away from the sailors!");
        send_narration(fd, "The monster lashes out with its tentacles! You dodge and strike back with all your might.");
        
        // Use character_fight method
        seamonster_mutex.lock();
        ps->player_char.character_fight(sea_monster, fd);
        seamonster_mutex.unlock();
        // Send updated character status
        ps->player_char.send_character(fd);
        sea_monster.send_character(fd);
        
        // Broadcast updates to everyone in the room
        broadcast_character_to_room(room_num, ps->player_char, player_name);
        broadcast_character_to_room(room_num, sea_monster, "");

        if (sea_monster.health <= 0) {
            send_narration(fd, "With a final blow, you slay the sea monster! The grateful sailors cheer and hand you a chest of gold.");
            // Remove sea monster from room (defeated)
            rooms_mutex.lock();
            auto& occ = room_occupants[6];
            occ.erase(remove(occ.begin(), occ.end(), "SEA MONSTER"), occ.end());
            rooms_mutex.unlock();


            // Make EVERYONE in the room eligible to loot
            rooms_mutex.lock();
            vector<string> room_players = room_occupants[room_num];
            rooms_mutex.unlock();
            players_mutex.lock();
            for (const string& name : room_players) {
                auto player_it = all_players.find(name);
                if (player_it != all_players.end()) {
                    player_it->second->seamonster_loot_eligible = true;
                }
            }
            players_mutex.unlock();

        } else if (ps->player_char.health <= 0) {
            send_narration(fd, "The sea monster overpowers you! You collapse onto the dock, defeated.");
        } else {
            send_narration(fd, "The battle rages on! Both you and the monster are wounded but still fighting.");
        }
        return;
    } else if (nightwitch_present) {
        // Fight the night witch
        send_narration(fd, "You confront the Night Witch! She raises her hands and dark energy crackles around her.");
        send_narration(fd, "She hurls a curse at you! You charge forward, trying to strike before she can cast another spell.");
        
        // Use character_fight method
        nightwitch_mutex.lock();
        ps->player_char.character_fight(night_witch, fd);
        nightwitch_mutex.unlock();
        
        // Send updated character status
        ps->player_char.send_character(fd);
        night_witch.send_character(fd);

        // Broadcast updates to everyone in the room
        broadcast_character_to_room(room_num, ps->player_char, player_name);
        broadcast_character_to_room(room_num, night_witch, "");
        
        if (night_witch.health <= 0) {
            send_narration(fd, "The Night Witch lets out a final shriek and dissolves into shadows! You've defeated her and can claim her treasures.");
            // Remove night witch from room (defeated)
            rooms_mutex.lock();
            auto& occ = room_occupants[4];
            occ.erase(remove(occ.begin(), occ.end(), "NIGHT WITCH"), occ.end());
            rooms_mutex.unlock();

            // Make EVERYONE in the room eligible to loot
            rooms_mutex.lock();
            vector<string> room_players = room_occupants[room_num];
            rooms_mutex.unlock();
            
            players_mutex.lock();
            for (const string& name : room_players) {
                auto player_it = all_players.find(name);
                if (player_it != all_players.end()) {
                    player_it->second->nightwitch_loot_eligible = true;
                }
            }
            players_mutex.unlock();


        } else if (ps->player_char.health <= 0) {
            send_narration(fd, "The Night Witch's curse strikes true! You fall to the ground, defeated by her dark magic.");
        } else {
            send_narration(fd, "The magical duel continues! Both you and the witch are wounded but the fight isn't over.");
        }
        return;
    }else if( ghost_present) {
        send_narration(fd, "You bravely face the Ghost haunting the cave!");
        send_narration(fd, "The Ghost wails and lunges at you!");

        ghost_mutex.lock();
        ps->player_char.character_fight(ghost, fd);
        ghost_mutex.unlock();

        ps->player_char.send_character(fd);
        ghost.send_character(fd);

        broadcast_character_to_room(room_num, ps->player_char, player_name);
        broadcast_character_to_room(room_num, ghost, "");

        if (ghost.health <= 0) {
            send_narration(fd, "You have defeated the Ghost! You can now loot its belongings.");
            rooms_mutex.lock();
            auto& occ = room_occupants[10];
            occ.erase(remove(occ.begin(), occ.end(), "GHOST"), occ.end());
            rooms_mutex.unlock();

            rooms_mutex.lock();
            vector<string> room_players = room_occupants[room_num];
            rooms_mutex.unlock();
            
            players_mutex.lock();
            for (const string& name : room_players) {
                auto player_it = all_players.find(name);
                if (player_it != all_players.end()) {
                    player_it->second->ghost_loot_eligible = true;
                }
            }
            players_mutex.unlock();
        } else if (ps->player_char.health <= 0) {
            send_narration(fd, "The Ghost overwhelms you! You collapse onto the ground, defeated.");
        } else {
            send_narration(fd, "The fight continues! Both you and the Ghost are wounded but still standing.");
        }
        return;
    } else if( zombie_present) {
        send_narration(fd, "You steel yourself to fight the Zombie in the desert!");
        send_narration(fd, "The Zombie shambles towards you, arms outstretched!");

        zombie_mutex.lock();
        ps->player_char.character_fight(zombie, fd);
        zombie_mutex.unlock();

        ps->player_char.send_character(fd);
        zombie.send_character(fd);

        broadcast_character_to_room(room_num, ps->player_char, player_name);
        broadcast_character_to_room(room_num, zombie, "");

        if (zombie.health <= 0) {
            send_narration(fd, "You have defeated the Zombie! You can now loot its belongings.");
            rooms_mutex.lock();
            auto& occ = room_occupants[8];
            occ.erase(remove(occ.begin(), occ.end(), "ZOMBIE"), occ.end());
            rooms_mutex.unlock();

            rooms_mutex.lock();
            vector<string> room_players = room_occupants[room_num];
            rooms_mutex.unlock();
            
            players_mutex.lock();
            for (const string& name : room_players) {
                auto player_it = all_players.find(name);
                if (player_it != all_players.end()) {
                    player_it->second->zombie_loot_eligible = true;
                }
            }
            players_mutex.unlock();
        } else if (ps->player_char.health <= 0) {
            send_narration(fd, "The Zombie overpowers you! You collapse onto the ground, defeated.");
        } else {
            send_narration(fd, "The fight continues! Both you and the Zombie are wounded but still standing.");
        }
        return;
    
    } else if( dwarf1_present) {
        send_narration(fd, "You confront the Dwarf guarding the farm!");
        send_narration(fd, "The Dwarf swings his axe at you!");

        dwarf1_mutex.lock();
        ps->player_char.character_fight(dwarf1, fd);
        dwarf1_mutex.unlock();

        ps->player_char.send_character(fd);
        dwarf1.send_character(fd);

        broadcast_character_to_room(room_num, ps->player_char, player_name);
        broadcast_character_to_room(room_num, dwarf1, "");

        if (dwarf1.health <= 0) {
            send_narration(fd, "You have defeated the Dwarf! You can now loot his belongings.");
            rooms_mutex.lock();
            auto& occ = room_occupants[7];
            occ.erase(remove(occ.begin(), occ.end(), "DWARF1"), occ.end());
            rooms_mutex.unlock();

            rooms_mutex.lock();
            vector<string> room_players = room_occupants[room_num];
            rooms_mutex.unlock();
            
            players_mutex.lock();
            for (const string& name : room_players) {
                auto player_it = all_players.find(name);
                if (player_it != all_players.end()) {
                    player_it->second->dwarf1_loot_eligible = true;
                }
            }
            players_mutex.unlock();
        } else if (ps->player_char.health <= 0) {
            send_narration(fd, "The Dwarf overpowers you! You collapse onto the ground, defeated.");
        } else {
            send_narration(fd, "The fight continues! Both you and the Dwarf are wounded but still standing.");
        }
        return;
    }else if( dwarf2_present) {
        send_narration(fd, "You confront the Dwarf guarding the farm!");
        send_narration(fd, "The Dwarf swings his axe at you!");

        dwarf2_mutex.lock();
        ps->player_char.character_fight(dwarf2, fd);
        dwarf2_mutex.unlock();

        ps->player_char.send_character(fd);
        dwarf2.send_character(fd);

        broadcast_character_to_room(room_num, ps->player_char, player_name);
        broadcast_character_to_room(room_num, dwarf2, "");

        if (dwarf2.health <= 0) {
            send_narration(fd, "You have defeated the Dwarf! You can now loot his belongings.");
            rooms_mutex.lock();
            auto& occ = room_occupants[3];
            occ.erase(remove(occ.begin(), occ.end(), "DWARF2"), occ.end());
            rooms_mutex.unlock();

            rooms_mutex.lock();
            vector<string> room_players = room_occupants[room_num];
            rooms_mutex.unlock();
            
            players_mutex.lock();
            for (const string& name : room_players) {
                auto player_it = all_players.find(name);
                if (player_it != all_players.end()) {
                    player_it->second->dwarf2_loot_eligible = true;
                }
            }
            players_mutex.unlock();
        } else if (ps->player_char.health <= 0) {
            send_narration(fd, "The Dwarf overpowers you! You collapse onto the ground, defeated.");
        } else {
            send_narration(fd, "The fight continues! Both you and the Dwarf are wounded but still standing.");
        }
        return;
    }else if( dwarf3_present) {
        send_narration(fd, "You confront the Dwarf guarding the farm!");
        send_narration(fd, "The Dwarf swings his axe at you!");

        dwarf3_mutex.lock();
        ps->player_char.character_fight(dwarf3, fd);
        dwarf3_mutex.unlock();

        ps->player_char.send_character(fd);
        dwarf3.send_character(fd);

        broadcast_character_to_room(room_num, ps->player_char, player_name);
        broadcast_character_to_room(room_num, dwarf3, "");

        if (dwarf3.health <= 0) {
            send_narration(fd, "You have defeated the Dwarf! You can now loot his belongings.");
            rooms_mutex.lock();
            auto& occ = room_occupants[1];
            occ.erase(remove(occ.begin(), occ.end(), "DWARF3"), occ.end());
            rooms_mutex.unlock();

            rooms_mutex.lock();
            vector<string> room_players = room_occupants[room_num];
            rooms_mutex.unlock();
            
            players_mutex.lock();
            for (const string& name : room_players) {
                auto player_it = all_players.find(name);
                if (player_it != all_players.end()) {
                    player_it->second->dwarf3_loot_eligible = true;
                }
            }
            players_mutex.unlock();
        } else if (ps->player_char.health <= 0) {
            send_narration(fd, "The Dwarf overpowers you! You collapse onto the ground, defeated.");
        } else {
            send_narration(fd, "The fight continues! Both you and the Dwarf are wounded but still standing.");
        }
        return;
    }else if( dwarf4_present) {
        send_narration(fd, "You confront the Dwarf guarding the farm!");
        send_narration(fd, "The Dwarf swings his axe at you!");

        dwarf4_mutex.lock();
        ps->player_char.character_fight(dwarf4, fd);
        dwarf4_mutex.unlock();

        ps->player_char.send_character(fd);
        dwarf4.send_character(fd);

        broadcast_character_to_room(room_num, ps->player_char, player_name);
        broadcast_character_to_room(room_num, dwarf4, "");

        if (dwarf4.health <= 0) {
            send_narration(fd, "You have defeated the Dwarf! You can now loot his belongings.");
            rooms_mutex.lock();
            auto& occ = room_occupants[11];
            occ.erase(remove(occ.begin(), occ.end(), "DWARF4"), occ.end());
            rooms_mutex.unlock();

            rooms_mutex.lock();
            vector<string> room_players = room_occupants[room_num];
            rooms_mutex.unlock();
            
            players_mutex.lock();
            for (const string& name : room_players) {
                auto player_it = all_players.find(name);
                if (player_it != all_players.end()) {
                    player_it->second->dwarf4_loot_eligible = true;
                }
            }
            players_mutex.unlock();
        } else if (ps->player_char.health <= 0) {
            send_narration(fd, "The Dwarf overpowers you! You collapse onto the ground, defeated.");
        } else {
            send_narration(fd, "The fight continues! Both you and the Dwarf are wounded but still standing.");
        }
        return;
    }else if( dwarf5_present) {
        send_narration(fd, "You confront the Dwarf guarding the farm!");
        send_narration(fd, "The Dwarf swings his axe at you!");

        dwarf5_mutex.lock();
        ps->player_char.character_fight(dwarf5, fd);
        dwarf5_mutex.unlock();

        ps->player_char.send_character(fd);
        dwarf5.send_character(fd);

        broadcast_character_to_room(room_num, ps->player_char, player_name);
        broadcast_character_to_room(room_num, dwarf5, "");

        if (dwarf5.health <= 0) {
            send_narration(fd, "You have defeated the Dwarf! You can now loot his belongings.");
            rooms_mutex.lock();
            auto& occ = room_occupants[12];
            occ.erase(remove(occ.begin(), occ.end(), "DWARF5"), occ.end());
            rooms_mutex.unlock();

            rooms_mutex.lock();
            vector<string> room_players = room_occupants[room_num];
            rooms_mutex.unlock();
            
            players_mutex.lock();
            for (const string& name : room_players) {
                auto player_it = all_players.find(name);
                if (player_it != all_players.end()) {
                    player_it->second->dwarf5_loot_eligible = true;
                }
            }
            players_mutex.unlock();
        } else if (ps->player_char.health <= 0) {
            send_narration(fd, "The Dwarf overpowers you! You collapse onto the ground, defeated.");
        } else {
            send_narration(fd, "The fight continues! Both you and the Dwarf are wounded but still standing.");
        }
        return;
    }else {
        err.send_emsg(7, fd); // No monster to fight
        return;
    }
}

void handle_pvp_fight(int fd, string player_name) {
    
    // Read target player name (32 bytes)
    char target_name[33];
    if(recv(fd, target_name, 32, MSG_WAITALL) != 32) {
        return;
    }
    target_name[32] = '\0';
    
    // Get both players
    players_mutex.lock();
    auto attacker_it = all_players.find(player_name);
    auto target_it = all_players.find(string(target_name));
    
    if (attacker_it == all_players.end() || target_it == all_players.end()) {
        players_mutex.unlock();
        err.send_emsg(6, fd); // Invalid target
        return;
    }
    
    player_state* attacker_ps = attacker_it->second;
    // Check if attacker is alive
     if (!(attacker_ps->player_char.flags & 0x80)) {
        players_mutex.unlock();
        err.send_emsg(9, fd);
        return;
    }

    player_state* target_ps = target_it->second;
    
    
    // Check if both players are in the same room
    if (attacker_ps->player_char.current_room != target_ps->player_char.current_room) {
        players_mutex.unlock();
        err.send_emsg(6, fd); // Target not in same room
        return;
    }
    
    // Check if target is alive
    if (!(target_ps->player_char.flags & 0x80)) {
        players_mutex.unlock();
        err.send_emsg(6, fd); // Target is dead
        return;
    }
    
    uint16_t room_num = attacker_ps->player_char.current_room;
    
    // Send narration to attacker
    char msg1[256];
    snprintf(msg1, sizeof(msg1), "You attack %s!", target_name);
    send_narration(fd, msg1);
    send_narration(fd, "Weapons clash as they try to defend themselves!");
    
    // Send narration to target
    char msg2[256];
    snprintf(msg2, sizeof(msg2), "%s attacks you!", player_name.c_str());
    target_ps->send_mutex.lock();
    send_narration(target_ps->socket_fd, msg2);
    send_narration(target_ps->socket_fd, "You are under attack! Defend yourself!");
    target_ps->send_mutex.unlock();
    
    // Execute the fight
    attacker_ps->player_char.character_fight(target_ps->player_char, fd);
    
    // Send updated characters to both players
    attacker_ps->player_char.send_character(fd);
    target_ps->send_mutex.lock();
    target_ps->player_char.send_character(target_ps->socket_fd);
    target_ps->send_mutex.unlock();
    
    players_mutex.unlock();
    // Broadcast updated characters to everyone in the room
    broadcast_character_to_room(room_num, attacker_ps->player_char, player_name);
    broadcast_character_to_room(room_num, target_ps->player_char, string(target_name));
    
    // Send outcome narration
    if (target_ps->player_char.health <= 0) {

        char msg3[256];
        snprintf(msg3, sizeof(msg3), "You have defeated %s!", target_name);
        send_narration(fd, msg3);
        
        target_ps->send_mutex.lock();
        snprintf(msg3, sizeof(msg3), "%s has defeated you!", player_name.c_str());
        send_narration(target_ps->socket_fd, msg3);
        target_ps->send_mutex.unlock();
    } else if (attacker_ps->player_char.health <= 0) {
        char msg3[256];
        snprintf(msg3, sizeof(msg3), "%s has defeated you!", target_name);
        send_narration(fd, msg3);
        
        target_ps->send_mutex.lock();
        snprintf(msg3, sizeof(msg3), "You have defeated %s!", player_name.c_str());
        send_narration(target_ps->socket_fd, msg3);
        target_ps->send_mutex.unlock();
    } else {
        send_narration(fd, "The fight continues! they were wounded but still standing.");
        target_ps->send_mutex.lock();
        send_narration(target_ps->socket_fd, "The fight continues! you are wounded but still standing.");
        target_ps->send_mutex.unlock();
    }
}



void handle_loot(int fd, string player_name) {

    char target_name[33];
    if(recv(fd, target_name, 32, MSG_WAITALL) != 32) {
        return;
    }
    target_name[32] = '\0';

    players_mutex.lock();
    auto it = all_players.find(player_name);
    if (it == all_players.end()) {
        players_mutex.unlock();
        err.send_emsg(3, fd);
        return;
    }
    player_state* ps = it->second;

    if (!(ps->player_char.flags & 0x80)) {
        players_mutex.unlock();
        err.send_emsg(9, fd);
        return;
    }
    uint16_t room_num = ps->player_char.current_room;
    players_mutex.unlock();

    // Check if looting a monster
    bool is_bandit = strcmp(target_name, "BANDIT") == 0;
    bool is_seamonster = strcmp(target_name, "SEA MONSTER") == 0;
    bool is_nightwitch = strcmp(target_name, "NIGHT WITCH") == 0;
    bool is_ghost = strcmp(target_name, "GHOST") == 0;
    bool is_zombie = strcmp(target_name, "ZOMBIE") == 0;
    bool is_dwarf1 = strcmp(target_name, "DWARF1") == 0;
    bool is_dwarf2 = strcmp(target_name, "DWARF2") == 0;
    bool is_dwarf3 = strcmp(target_name, "DWARF3") == 0;
    bool is_dwarf4 = strcmp(target_name, "DWARF4") == 0;
    bool is_dwarf5 = strcmp(target_name, "DWARF5") == 0;
    
    if (is_bandit || is_seamonster || is_nightwitch || is_ghost || is_zombie || is_dwarf1 || is_dwarf2 || is_dwarf3 || is_dwarf4 || is_dwarf5) {
        // Monster looting logic
        rooms_mutex.lock();
        auto& occupants = room_occupants[room_num];
        bool monster_present = false;
        for (const string& occ : occupants) {
            if ((is_bandit && occ == "BANDIT") || 
                (is_seamonster && occ == "SEA MONSTER") ||
                (is_nightwitch && occ == "NIGHT WITCH")
                || (is_ghost && occ == "GHOST")
                || (is_zombie && occ == "ZOMBIE")
                || (is_dwarf1 && occ == "DWARF1")
                || (is_dwarf2 && occ == "DWARF2")
                || (is_dwarf3 && occ == "DWARF3")
                || (is_dwarf4 && occ == "DWARF4")
                || (is_dwarf5 && occ == "DWARF5")){
                monster_present = true;
                break;
            }
        }
        rooms_mutex.unlock();

        if (monster_present) {
            err.send_emsg(3, fd); // Monster still alive
            return;
        }

        uint16_t loot_gold = 0;
        if (is_bandit) {
            if (!ps->bandit_loot_eligible) {
                err.send_emsg(3, fd);
                return;
            }
            if(bandit.gold <=10){
                loot_gold = bandit.gold;
            }
            else{
                loot_gold = bandit.gold/2;
            }
            if(loot_gold == 0){
                ps->bandit_loot_eligible = false;
            }
            
            bandit.gold -= loot_gold;
        } else if (is_seamonster) {
            if (!ps->seamonster_loot_eligible) {
                err.send_emsg(3, fd);
                return;
            }
            else{
                loot_gold = sea_monster.gold;
            }
            if(loot_gold == 0){
                ps->seamonster_loot_eligible = false;
            }
            sea_monster.gold = 0;
        } else if (is_nightwitch) {
            if (!ps->nightwitch_loot_eligible) {
                err.send_emsg(3, fd);
                return;
            }
            if(night_witch.gold <=10){
                loot_gold = night_witch.gold;
            }
            else{
                loot_gold = night_witch.gold/3;
            }
            if(loot_gold == 0){
                ps->nightwitch_loot_eligible = false;
            }
            night_witch.gold -= loot_gold;
        }else if (is_ghost) {
            if (!ps->ghost_loot_eligible) {
                err.send_emsg(3, fd);
                return;
            }
            loot_gold = ghost.gold;
            ghost.gold = 0;
            ps->ghost_loot_eligible = false;
        }else if (is_zombie) {
            if (!ps->zombie_loot_eligible) {
                err.send_emsg(3, fd);
                return;
            }
            loot_gold = zombie.gold;
            zombie.gold = 0;
            ps->zombie_loot_eligible = false;
        }else if (is_dwarf1) {
            if (!ps->dwarf1_loot_eligible) {
                err.send_emsg(3, fd);
                return;
            }
            loot_gold = dwarf1.gold;
            dwarf1.gold = 0;
            ps->dwarf1_loot_eligible = false;
        }else if (is_dwarf2){
            if (!ps->dwarf2_loot_eligible) {
                err.send_emsg(3, fd);
                return;
            }
            loot_gold = dwarf2.gold;
            dwarf2.gold = 0;
            ps->dwarf2_loot_eligible = false;
        }else if (is_dwarf3){
            if (!ps->dwarf3_loot_eligible) {
                err.send_emsg(3, fd);
                return;
            }
            loot_gold = dwarf3.gold;
            dwarf3.gold = 0;
            ps->dwarf3_loot_eligible = false;
        }else if (is_dwarf4){
            if (!ps->dwarf4_loot_eligible) {
                err.send_emsg(3, fd);
                return;
            }
            loot_gold = dwarf4.gold;
            dwarf4.gold = 0;
            ps->dwarf4_loot_eligible = false;
        }else if (is_dwarf5){
            if (!ps->dwarf5_loot_eligible) {
                err.send_emsg(3, fd);
                return;
            }
            loot_gold = dwarf5.gold;
            dwarf5.gold = 0;
            ps->dwarf5_loot_eligible = false;
        }
        
        if (loot_gold == 0) {
            err.send_emsg(3, fd);
            return;
        }
        
        ps->player_char.gold += loot_gold; 
        broadcast_character_to_room(room_num, ps->player_char, "");

        if (is_bandit) {
            broadcast_character_to_room(room_num, bandit, "");
        } else if (is_seamonster) {
            broadcast_character_to_room(room_num, sea_monster, "");
        } else if (is_nightwitch) {
            broadcast_character_to_room(room_num, night_witch, ""); 
        }else if (is_ghost) {
            broadcast_character_to_room(room_num, ghost, "");
        }else if (is_zombie) {
            broadcast_character_to_room(room_num, zombie, "");
        }else if (is_dwarf1) {
            broadcast_character_to_room(room_num, dwarf1, "");
        }else if (is_dwarf2) {
            broadcast_character_to_room(room_num, dwarf2, "");
        }else if (is_dwarf3) {
            broadcast_character_to_room(room_num, dwarf3, "");
        }else if (is_dwarf4) {
            broadcast_character_to_room(room_num, dwarf4, "");
        }else if (is_dwarf5) {
            broadcast_character_to_room(room_num, dwarf5, "");
        }
        
        const char* msg;
        if (is_bandit) msg = "You search the fallen bandit and find a pouch of gold!";
        else if (is_seamonster) msg = "The rescued sailors hand you a chest of gold in gratitude!";
        else if(is_nightwitch) msg = "You search the witch's lair and discover her hidden treasures!";
        else if(is_ghost) msg = "You search the remains of the Ghost and find some scattered coins!";
        else if(is_zombie) msg = "You search the remains of the Zombie and find some scattered coins!";
        else if(is_dwarf1) msg = "You search the fallen Dwarf and find some of his gold!";
        else if(is_dwarf2) msg = "You search the fallen Dwarf and find some of his gold!";
        else if(is_dwarf3) msg = "You search the fallen Dwarf and find some of his gold!";
        else if(is_dwarf4) msg = "You search the fallen Dwarf and find some of his gold!";
        else if(is_dwarf5) msg = "You search the fallen Dwarf and find some of his gold!";
        send_narration(fd, msg);
    } 
    else {
        // Try to loot a dead player
        players_mutex.lock();
        auto target_it = all_players.find(string(target_name));
        if (target_it == all_players.end()) {
            players_mutex.unlock();
            err.send_emsg(3, fd);
            return;
        }
        player_state* target_ps = target_it->second;
        
        // Check if target is in same room
        if (target_ps->player_char.current_room != room_num) {
            players_mutex.unlock();
            err.send_emsg(3, fd);
            return;
        }
        
        // Check if target is dead
        bool is_dead = (target_ps->player_char.health <= 0) || !(target_ps->player_char.flags & 0x80);
        
        if (!is_dead) {
            players_mutex.unlock();
            err.send_emsg(3, fd);
            return;
        }
        
        // Loot ALL of their gold
        uint16_t loot_gold = target_ps->player_char.gold;
        
        if (loot_gold == 0) {
            players_mutex.unlock();
            err.send_emsg(3, fd);
            return;
        }
        
        // Update gold amounts
        target_ps->player_char.gold = 0;
        ps->player_char.gold += loot_gold;


        ps->send_mutex.lock();
        ps->player_char.send_character(fd);
        ps->send_mutex.unlock();

        target_ps->send_mutex.lock();
        target_ps->player_char.send_character(target_ps->socket_fd);
        target_ps->send_mutex.unlock();

        players_mutex.unlock();  // MOVED: unlock BEFORE broadcasts
        
        // Broadcast the updated characters to everyone in the room
        broadcast_character_to_room(room_num, target_ps->player_char, string(target_name));
        broadcast_character_to_room(room_num, ps->player_char, player_name);
        
        send_narration(fd, "You search the fallen player and take their gold!");
    }
}


// -------------------- handle_client --------------------
void handle_client(int client_fd, struct sockaddr_in client_address) {
    
    printf("New connection from %s:%d\n", inet_ntoa(client_address.sin_addr), ntohs(client_address.sin_port));
    
    version currentv(2, 3);
    currentv.send_version(client_fd);
    global_game->send_game(client_fd);
    
    character player;
    bool accepted = false;
    
    while (!accepted) {
        printf("Waiting to receive character...\n");
        if (!player.receive_character(client_fd)) {
            printf("Failed to receive character, closing connection\n");
            close(client_fd);
            return;
        }
        
        players_mutex.lock();
        if (all_players.find(string(player.name)) != all_players.end()) {
            players_mutex.unlock();
            printf("Player %s already exists, sending error\n", player.name);
            err.send_emsg(2, client_fd);
            continue;
        }
        players_mutex.unlock();
        
        uint16_t total = player.attack + player.defense + player.regen;
        printf("Character %s: total stats = %d (limit: %d)\n", player.name, total, global_game->initial_pts);
        if (total <= global_game->initial_pts) {
            printf("Accepting character %s\n", player.name);
            acc.send_accepted(client_fd, 10);
            player.set_started();
            player.health = 100;
            player.gold = 73;
            player.current_room = 1;
            player.send_character(client_fd);
            accepted = true;
        } else {
            printf("Stats exceed limit, sending error\n");
            err.send_emsg(4, client_fd);
        }
    }
    
    printf("Waiting for START message...\n");
    uint8_t start_type;
    if(recv(client_fd, &start_type, 1, 0) <= 0) {
        printf("Failed to receive START message\n");
        close(client_fd);
        return;
    }
    if (start_type != 6) {
        printf("Expected START (6), got %d\n", start_type);
        close(client_fd);
        return;
    }
    
    printf("Player %s started!\n", player.name);
    
    player_state* ps = new player_state;
    ps->player_char = player;
    ps->socket_fd = client_fd;
    ps->active = true;
    players_mutex.lock();
    all_players[string(player.name)] = ps;
    players_mutex.unlock();
    
    rooms_mutex.lock();
    room_occupants[1].push_back(string(player.name));
    rooms_mutex.unlock();
    
    send_room_entry(client_fd, souk, string(player.name), ps);
    
    // Notify others in the room
    broadcast_character_to_room(1, ps->player_char, string(player.name));
    
    while (ps->active) {
        uint8_t msg_type;
        int bytes = recv(client_fd, &msg_type, 1, 0);
        if (bytes <= 0) {
            if (bytes == 0) {
                printf("Client %s closed connection (recv returned 0)\n", player.name);
            } else {
                perror("recv failed");
            }
            break;
        }
        
        if (msg_type == 0) {
            break;
        }
            
        switch(msg_type) {
            case 1:
                handle_message_relay(client_fd, string(player.name));
                break;
            case 2:
                handle_room_change(client_fd, string(player.name));
                break;
            case 3:
                handle_fight(client_fd, string(player.name));
                break;
            case 4:
                handle_pvp_fight(client_fd, string(player.name));
                break;
            case 5:
                handle_loot(client_fd, string(player.name));
                break;
            case 12:
                printf("Player %s requested to leave (type 12).\n", player.name);
                ps->active = false;
                // Optionally send a goodbye message or accepted message
                acc.send_accepted(client_fd, 12);
                break;
            default:
                printf("Unknown message type: %d\n", msg_type);
                break;
        }
    }
    
    cleanup_player(string(player.name));
    close(client_fd);
    printf("Client %s disconnected\n", player.name);
}

// -------------------- main --------------------
int main(int argc, char* argv[]) {
    signal(SIGPIPE, SIG_IGN);

    char description[] =  
                "                                      \n"
                "  _____  __      _ _    _             \n"
                " |_   _|/ _|    (_) |  (_)            \n"
                "   | | | |_ _ __ _| | ___ _   _  __ _ \n"
                "   | | |  _| '__| | |/ / | | | |/ _` |\n"
                "  _| |_| | | |  | |   <| | |_| | (_| |\n"
                " |_____|_| |_|  |_|_|\\_\\_|\\__, |\\__,_|\n"
                "                           __/ |      \n"
                "                          |___/       \n"
                "You arrived in North Africa as a curious tourist, drawn by the markets, spices, and ancient ruins. But after touching a strange artifact in a dusty bazaar, your world shifted.The sounds of cars and chatter disappeared, replaced by horse hooves, the call of merchants, and the clang of blacksmiths. Slowly, you realize the truth: you are not in your century anymore. You are in the year 1001, five centuries before Columbus ever dreamed of crossing the ocean to America. You are alone, far from home, in a time long before your own. The question is not where you are, but when you will survive.\n";

    global_game = new game(103, 65535, description);
    setup_rooms();

    struct sockaddr_in sin;
    int sockd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockd == -1) {
        perror("socket");
        return 1;
    }
    
    int opt = 1;
    setsockopt(sockd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    sin.sin_family = AF_INET;
    sin.sin_port = htons(atoi(argv[1]));
    sin.sin_addr.s_addr = htonl(INADDR_ANY);
    
    if (bind(sockd, (struct sockaddr*)&sin, sizeof(sin))) {
        perror("bind");
        return 1;
    }

    if (listen(sockd, 5)) {
        perror("listen");
        return 1;
    }
    
    printf("Lurk server listening on port %s\n", argv[1]);
    
    while (1) {
        socklen_t sinlen = sizeof(sin);
        int ac = accept(sockd, (struct sockaddr*)&sin, &sinlen);
        if (ac > 0) {
            thread client_thread(handle_client, ac, sin);
            client_thread.detach();
        }
    }

    return 0;
}
