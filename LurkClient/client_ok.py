import sys
from PyQt5 import QtWidgets, QtCore, QtGui
from PyQt5.QtCore import QThread, pyqtSignal
from socket import *
from struct import *
import json



#Import the generated UI class
from client_ui import Ui_Dialog
from struct import pack


class GameState():
    #--------------player data-------
    player_name=''
    player_attack=0
    player_defense=0
    player_regen=0
    player_health=100
    player_gold=0
    player_flags=0xff
    player_room=0
    player_description=''
    player_description_length=0

    #--------save socket to be able to use it later-----
    player_socket=0

    #-------------current room data----------------
    room_name=''
    room_number=0
    room_description=''

    #------------character_dictionary--------------
    player_Dict = {'character name':["list of character's data"]}

    #------------Connections list------------------
    Connections_list = ["accessible rooms"]

    #------------Messages List--------------------
    Messages_list = ["messages"]

    #------------some Game attributs--------------
    version=''
    initial_points=0
    stat_limit=0
    game_description=''
    
    
#---------------------------------functions to parse received messages read from server-------------------------------------

#------------read recieved type and return it--------------------
def parse_recieved_type(data):
    return data[0]
# ---------------- parse flags ----------------
def parse_flags(byte_val):
    #Parse the 5 main flags from a single byte.
    return {
        "alive": bool(byte_val & 0b10000000),
        "join_battle": bool(byte_val & 0b01000000),
        "monster": bool(byte_val & 0b00100000),
        "started": bool(byte_val & 0b00010000),
        "ready": bool(byte_val & 0b00001000),
        # The lowest 3 bits are reserved
    }

#------------reads Message: type 1------------------------
def parse_message(data):
    # Unpack the header (67 bytes)
    header = unpack("<BH32s30sBB", data[:67])

    recipient = header[2].decode("utf-8", errors="replace").rstrip('\x00')
    sender = header[3].decode("utf-8", errors="replace").rstrip('\x00')
    msg_length = header[1]

    actual_message = data[67:67+msg_length].decode("utf-8", errors="ignore")


    return {
        "type": header[0],
        "length": msg_length,
        "recipient": recipient,
        "sender": sender,
        "narration_marker": (header[4], header[5]),
        "message": actual_message 
    }
# ----------------parse type 6 start----------------
def parse_start(data):
    # Only the type byte is needed
    return {"type": data[0]}

# ----------------parse error type 7----------------
def parse_error(data):
    err_type, code, msg_len = unpack("<BBH", data[:4])
    msg = data[4:4+msg_len].decode("utf-8", errors="replace")
    return {
        "type": err_type,
        "code": code,
        "message": msg
    }

# ----------------parse accept type 8----------------
def parse_accept(data):
    msg_type, action_type = unpack("<BB", data[:2])
    return {
        "type": msg_type,
        "action_type": action_type
    }

# ---------------- parse room type 9 ----------------
def parse_room(data):
    room_type, room_number = unpack("<BH", data[:3])
    room_name = data[3:35].decode("utf-8", errors="replace").rstrip("\x00")
    desc_len = unpack("<H", data[35:37])[0]
    room_desc = data[37:37+desc_len].decode("utf-8", errors="replace")
    return {
        "type": room_type,
        "number": room_number,
        "name": room_name,
        "description": room_desc
    }

# ---------------- parse connection type 2----------------
def parse_connection(data):
    conn_type, room_number = unpack("<BH", data[:3])
    room_name = data[3:35].decode("utf-8", errors="replace").rstrip("\x00")
    desc_len = unpack("<H", data[35:37])[0]
    room_desc = data[37:37+desc_len].decode("utf-8", errors="replace")
    return {
        "type": 13,
        "number": room_number,
        "name": room_name,
        "description": room_desc
    }


# ---------------- parse character type 10 ----------------
def parse_character(data):

    char_type = data[0]
    name = data[1:33].decode("utf-8", errors="replace").rstrip("\x00")
    flags = parse_flags(data[33])
    attack, defense, regen, health, gold, room_number = unpack("<HHHhHH", data[34:46])
    desc_len = unpack("<H", data[46:48])[0]
    description = data[48:].decode("utf-8", errors="replace")
    
    return {
        "type": char_type,
        "name": name,
        "flags": flags,
        "attack": attack,
        "defense": defense,
        "regen": regen,
        "health": health,
        "gold": gold,
        "room_number": room_number,
        "description": description
    }


#------------------------------------------Thread---------------------------------------------------

class NetworkThread(QThread):

    message_received = pyqtSignal(dict)
    room_received = pyqtSignal(dict)
    character_received = pyqtSignal(dict)
    connection_received = pyqtSignal(dict)
    error_received = pyqtSignal(dict)
    accept_received = pyqtSignal(dict)
    version_received = pyqtSignal(str)


    def __init__(self, socket):
        super().__init__()
        self.socket = socket
        self.running = True


    def run(self):
        while self.running:
            try:
                # Receive message type
                type_byte = self.socket.recv(1, MSG_WAITALL)
                if not type_byte:  # Connection closed
                    break
                    
                msg_type = type_byte[0]
                
                match msg_type:
                    case 1:  # message
                        data = type_byte + self.socket.recv(66, MSG_WAITALL)
                        msg_length = unpack("<H", data[1:3])[0]
                        data += self.socket.recv(msg_length, MSG_WAITALL)
                        msg_dict = parse_message(data) 
                        self.message_received.emit(msg_dict)

                    case 7:  # error
                        header = self.socket.recv(3, MSG_WAITALL)
                        code, msg_len = unpack("<BH", header)
                        msg = self.socket.recv(msg_len, MSG_WAITALL).decode("utf-8", errors="replace")
                        self.error_received.emit({"type": 7, "code": code, "message": msg})
                        
                    case 9:  # room
                        data = type_byte + self.socket.recv(36, MSG_WAITALL)
                        desc_len = unpack("<H", data[35:37])[0]
                        data += self.socket.recv(desc_len, MSG_WAITALL)
                        room_dict = parse_room(data)
                        self.room_received.emit(room_dict)
                    case 8: #accept
                        data = type_byte + self.socket.recv(1, MSG_WAITALL)
                        accept_dict = parse_accept(data)
                        self.accept_received.emit(accept_dict)

                        
                    case 10:  # character
                        data = type_byte + self.socket.recv(47, MSG_WAITALL)
                        desc_len = unpack("<H", data[46:48])[0]
                        data += self.socket.recv(desc_len, MSG_WAITALL)
                        char_dict = parse_character(data)
                        self.character_received.emit(char_dict)
                        
                    case 13:  # connection
                        data = type_byte + self.socket.recv(36, MSG_WAITALL)
                        desc_len = unpack("<H", data[35:37])[0]
                        data += self.socket.recv(desc_len, MSG_WAITALL)
                        conn_dict = parse_connection(data)
                        self.connection_received.emit(conn_dict)
                    case 14:#version
                        v = type_byte + self.socket.recv(4, MSG_WAITALL)
                        game_version = str(v[1])+'.'+str(v[2])
                        self.version_received.emit(game_version)

                        
                    case _:  # Default case for unknown message types
                        print(f"Unknown message type: {msg_type}")
                        
            except Exception as e:
                print(f"Network thread error: {e}")
                break
    
    def stop(self):
        self.running = False

                    
#---------------------------application class that inherits from QDialog and hosts the UI----------------------

class ClientApp(QtWidgets.QDialog):
    game_state=None
    other_characters = {}
    available_rooms = []
    
    
    
    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)
        self.game_state = GameState()
        
        
        # Setup the UI
        self.ui = Ui_Dialog()
        self.ui.setupUi(self)

        self.ui.MainDisplayTextEdit.setFont(QtGui.QFont("Courier New"))


         # Prevent any button from being the default (triggered by Enter)
        self.ui.ConnectButton.setAutoDefault(False)
        self.ui.sendButton.setAutoDefault(False)

         #Disable default button behavior for the entire dialog
        self.ui.ConnectButton.setDefault(False)
        self.ui.sendButton.setDefault(False)
        
        # Disconnect return key from all input fields
        self.ui.port_input.returnPressed.connect(lambda: None)
        self.ui.General_input.returnPressed.connect(lambda: None)
        
        self.ui.ConnectButton.clicked.connect(self.connect_and_load_game)
        self.ui.QuitButton.clicked.connect(self.send_leave)


    #------------Game display function: used only once at connection-----------------
    def displayGame(self, v, ip, sl, dl, gd, port):
        self.game_state.version=v
        self.game_state.initial_points=ip
        self.game_state.stat_limit=sl
        self.game_state.description_length=dl
        self.game_state.game_description=gd
        display_text = f"Lurk Server running Lurk Version {self.game_state.version}\n"
        display_text += f"Connected to port {port}!\n\n"
        display_text += f"Stat Limit: {self.game_state.stat_limit}\n"
        display_text += f"Initial Points: {self.game_state.initial_points}\n"
        display_text += f"Description: \n{self.game_state.game_description}"
        self.ui.MainDisplayTextEdit.setText(display_text)
        self.ui.MainDisplayTextEdit.moveCursor(QtGui.QTextCursor.End)  # auto-scroll to the bottom
        return True
    
    def get_player(self):
        self.ui.InstructionTextEdit.setText("Enter name in the box below")
        self.ui.sendButton.clicked.connect(self.handle_name_input)
        return True

    def handle_name_input(self):
        self.game_state.player_name = self.ui.General_input.text().strip()
        trimmed = self.game_state.player_name.strip()
        if trimmed != '':
            self.ui.General_input.clear()
            self.ui.InstructionTextEdit.setText("Enter your description")
            self.ui.sendButton.clicked.disconnect(self.handle_name_input)
            self.ui.sendButton.clicked.connect(self.handle_desc_input)
        else:
            self.ui.InstructionTextEdit.setText("Enter A VALID name in the box below")
            self.ui.General_input.clear()
            self.game_state.player_name = self.ui.General_input.text().strip()



    def handle_desc_input(self):
        self.game_state.player_description = self.ui.General_input.text()
        if self.game_state.player_description.isdigit():
            self.ui.InstructionTextEdit.setText("Enter A VALID description, or leave it empty")
            self.ui.General_input.clear()
        else:
            self.game_state.player_description_length=len(self.game_state.player_description)
            self.ui.General_input.clear()
            self.ui.InstructionTextEdit.setText("Enter your attack, less than "+str( self.game_state.initial_points))
            self.ui.sendButton.clicked.disconnect(self.handle_desc_input)
            self.ui.sendButton.clicked.connect(self.handle_attack_input)
            
        
    def handle_attack_input(self):
        self.game_state.player_attack = self.ui.General_input.text()
        if(self.game_state.player_attack.isdigit() and int(self.game_state.player_attack) <= self.game_state.initial_points):
            self.game_state.player_attack=int(self.game_state.player_attack)
            self.ui.General_input.clear()
            self.ui.InstructionTextEdit.setText("Enter your defense, less than "+str(self.game_state.initial_points-self.game_state.player_attack))
            self.ui.sendButton.clicked.disconnect(self.handle_attack_input)
            self.ui.sendButton.clicked.connect(self.handle_defense_input)
        else:
            self.ui.General_input.clear()

        
    def handle_defense_input(self):
        self.game_state.player_defense = self.ui.General_input.text()
        if(self.game_state.player_defense.isdigit() and int(self.game_state.player_defense) <= self.game_state.initial_points-self.game_state.player_attack):
            self.game_state.player_defense=int(self.game_state.player_defense)
            self.ui.General_input.clear()
            self.ui.InstructionTextEdit.setText("Enter your regen, less than "+str(self.game_state.initial_points-self.game_state.player_attack-self.game_state.player_defense))
            self.ui.sendButton.clicked.disconnect(self.handle_defense_input)
            self.ui.sendButton.clicked.connect(self.handle_regen_input)
        else:
            self.ui.General_input.clear()
            
    
    
    def handle_regen_input(self):
        self.game_state.player_regen = self.ui.General_input.text()
        if(self.game_state.player_regen.isdigit() and int(self.game_state.player_regen) <= self.game_state.initial_points-self.game_state.player_attack-self.game_state.player_defense):
            self.game_state.player_regen=int(self.game_state.player_regen)
            self.ui.General_input.clear()
            self.ui.sendButton.clicked.disconnect(self.handle_regen_input)
            self.ui.InstructionTextEdit.setText("Got your character!!")

            self.send_player()


        else:
            self.ui.General_input.clear()

    def send_player(self):

        try:
            name_bytes = self.game_state.player_name.encode('utf-8')
            desc_bytes = self.game_state.player_description.encode('utf-8')
            
            p = f"<B32sBHHHhHHH{len(desc_bytes)}s"
            packet = pack(p, 10, name_bytes, self.game_state.player_flags, 
                        self.game_state.player_attack, self.game_state.player_defense, 
                        self.game_state.player_regen, self.game_state.player_health, 
                        self.game_state.player_gold, self.game_state.room_number, 
                        len(desc_bytes), desc_bytes)
            
            self.game_state.player_socket.send(packet)  

        except Exception as e:
            print(f"Error sending character: {e}")        

        self.network_thread = NetworkThread(self.game_state.player_socket)
        self.network_thread.room_received.connect(self.handle_room_received)
        self.network_thread.character_received.connect(self.handle_character_received)
        self.network_thread.connection_received.connect(self.handle_connection_received)
        self.network_thread.message_received.connect(self.handle_message_received)
        self.network_thread.error_received.connect(self.handle_error_received)
        self.network_thread.version_received.connect(self.handle_version_received)
        self.network_thread.start()

        self.ui.MainDisplayTextEdit.append("type start, to start game")
        self.ui.MainDisplayTextEdit.moveCursor(QtGui.QTextCursor.End)

        try:
            self.ui.sendButton.clicked.disconnect()  # Disconnect ALL old handlers
        except:
            pass  
            
        self.ui.sendButton.clicked.connect(self.handle_game_input)
        self.ui.InstructionTextEdit.setText("Type commands (help for list):")
        

#--------------------------Handle the second part of sending a message----------------------------------
    def handle_message_input(self):
        message = self.ui.General_input.text().strip()
        self.ui.General_input.clear()
        
        if message:
            self.send_message(self.current_msg_recipient, message)
        
        # Reconnect to game input handler
        try:
            self.ui.sendButton.clicked.disconnect(self.handle_message_input)
        except:
            pass
        self.ui.sendButton.clicked.connect(self.handle_game_input)
        self.ui.InstructionTextEdit.setText("Type commands (help for list):")

#----------------send message packet---------------------------
    def send_message(self, recipient_name, message):
        try:
            recipient_bytes = recipient_name.encode('utf-8')
            recipient_bytes = recipient_bytes.ljust(32, b'\x00')
            
            sender_bytes = self.game_state.player_name.encode('utf-8')
            sender_bytes = sender_bytes[:30]  # Truncate if too long
            sender_bytes = sender_bytes.ljust(30, b'\x00')
            
            # Narration marker bytes (bytes 65-66) - both 0 for player messages
            narration_bytes = b'\x00\x00'
            message_bytes = message.encode('utf-8')
            message_length = len(message_bytes)
            
            packet = pack(f'<BH32s30sBB{message_length}s', 1, message_length, recipient_bytes, sender_bytes, 0, 0, message_bytes)
            
            self.game_state.player_socket.send(packet)
            self.ui.MessagesTextEdit.append(f"Message sent to {recipient_name}: {message}")
            self.ui.MessagesTextEdit.moveCursor(QtGui.QTextCursor.End)
            
        except Exception as e:
            print(f"Error sending message: {e}")
            self.ui.MainDisplayTextEdit.append(f"X Failed to send message: {e}")
            self.ui.MainDisplayTextEdit.moveCursor(QtGui.QTextCursor.End)
            

    #------------------Now handling received things-------------Names are intuitive so no need for comments
    def handle_room_received(self, room_data):
        self.game_state.room_name = room_data['name']
        self.game_state.room_number = room_data['number']
        self.game_state.room_description = room_data['description']
        
        self.ui.MainDisplayTextEdit.append(f"\n{'='*50}")
        self.ui.MainDisplayTextEdit.append(f"{room_data['name']}")
        self.ui.MainDisplayTextEdit.append(f"{'='*50}")
        self.ui.MainDisplayTextEdit.append(f"{room_data['description']}")
        self.ui.MainDisplayTextEdit.moveCursor(QtGui.QTextCursor.End)

    def handle_character_received(self, char_data):
        if not char_data['flags']['alive']:
                char_data['attack']=str(char_data['attack'])+" | Dead ☠ "
        if char_data['flags']['monster']:
                char_data['attack']=str(char_data['attack'])+ " | Monster ☣ "
        if char_data['name'] == self.game_state.player_name:
            self.game_state.player_health = char_data['health']
            self.game_state.player_gold = char_data['gold']
            self.game_state.player_room=char_data['room_number']
            self.ui.PlayerStatsTextEdit.setText(str(self.game_state.player_name)+
                                                "  | ⚔ ATK: "+str(self.game_state.player_attack)+"  |"+
                                                " ⛨ DEF: "+str(self.game_state.player_defense)+"  │"
                                                " ✨ REG: "+str(self.game_state.player_regen)+"  |"
                                                " ♥ HP: "+str(self.game_state.player_health)+" │"
                                                " Gold: "+str(self.game_state.player_gold)+" |"
                                                " Cur Rm:  "+str(self.game_state.player_room))

        else:
            char_name = char_data['name']
            char_room = char_data['room_number']
            flags = char_data['flags']
            
            # Determine if player entered or left the room
            in_current_room = char_room == self.game_state.player_room
            already_tracked = char_name in self.other_characters

            if in_current_room and not already_tracked:
                # New player entered
                self.other_characters[char_name] = char_data
                if  not flags['monster']:
                    self.ui.MainDisplayTextEdit.append(f"--> {char_name} is in the room!")
                    self.ui.MainDisplayTextEdit.moveCursor(QtGui.QTextCursor.End)
                self.ui.CharactersTextEdit.clear()
                for char in self.other_characters.values():
                        self.ui.CharactersTextEdit.append((str(char['name'])+
                                                    "  | ⚔ "+str(char['attack'])+" |"+
                                                    " ⛨ "+str(char['defense'])+" │"
                                                    " ✨ "+str(char['regen'])+" |"
                                                    " ♥ "+str(char['health'])+" │"
                                                    +str(char['gold'])+" $\n"))
            elif not in_current_room and already_tracked:
                # Player left the room
                self.other_characters.pop(char_name)
                self.ui.MainDisplayTextEdit.append(f"<-- {char_name} left to room {char_room}")
                self.ui.CharactersTextEdit.clear()
                for char in self.other_characters.values():
                        self.ui.CharactersTextEdit.append((str(char['name'])+
                                                    "  | ⚔ "+str(char['attack'])+" |"+
                                                    " ⛨ "+str(char['defense'])+" │"
                                                    " ✨ "+str(char['regen'])+" |"
                                                    " ♥ "+str(char['health'])+" │"
                                                    +str(char['gold'])+" $\n"))

            else:
                # Update character info if already in room
                self.other_characters[char_name] = char_data

                self.ui.CharactersTextEdit.clear()
                for char in self.other_characters.values():
                        self.ui.CharactersTextEdit.append((str(char['name'])+
                                                    "  | ⚔ "+str(char['attack'])+" |"+
                                                    " ⛨ "+str(char['defense'])+" │"
                                                    " ✨ "+str(char['regen'])+" |"
                                                    " ♥ "+str(char['health'])+" │"
                                                    +str(char['gold'])+" $\n"))

            
            
        self.ui.CharactersTextEdit.moveCursor(QtGui.QTextCursor.End)

    def handle_connection_received(self, conn_data):
        #print(f"Received CONNECTION: {conn_data}")
        self.available_rooms.append(int(conn_data['number']))
        self.ui.AvailableRoomsTextEdit.append(f"\n{conn_data['number']}: {conn_data['name']}")
        self.ui.AvailableRoomsTextEdit.moveCursor(QtGui.QTextCursor.End)
        self.ui.MainDisplayTextEdit.append(f"\nConnecting room#{conn_data['number']}: {conn_data['name']}\n Description: {conn_data['description']}")
        self.ui.MainDisplayTextEdit.moveCursor(QtGui.QTextCursor.End)

#------------------handle version message will be called only if the server is older than 2.3-----------------------
    def handle_version_received(self, version):
        self.ui.MainDisplayTextEdit.append(f"Server Lurk Version: {version}")
        self.ui.MainDisplayTextEdit.moveCursor(QtGui.QTextCursor.End)
        self.game_state.version = version





    def handle_message_received(self, msg_data):
        #print(f"Received MESSAGE: {msg_data}")
        is_narration = (msg_data['narration_marker'][0] == 0 and msg_data['narration_marker'][1] == 1)
        
        if is_narration:
            text = f"\n***NARRATOR: {msg_data['message']} ***"
            self.ui.MainDisplayTextEdit.append(text)
            self.ui.MainDisplayTextEdit.moveCursor(QtGui.QTextCursor.End)

        else:
            text = f"\n{msg_data['sender']}: {msg_data['message']}"
            self.ui.MessagesTextEdit.append(text)
            self.ui.MessagesTextEdit.moveCursor(QtGui.QTextCursor.End)

    def handle_error_received(self, error_data):
        #print(f"Received ERROR: {error_data}")
        self.ui.MainDisplayTextEdit.append(f"\nX ERROR: {error_data['message']}")
        self.ui.MainDisplayTextEdit.moveCursor(QtGui.QTextCursor.End)

    

    #-------------------------------handling player input-------------------------------
    def handle_game_input(self):
        command = self.ui.General_input.text().strip()
        self.ui.General_input.clear()
        
        if not command:
            return
        
        parts = command.split()
        cmd = parts[0].lower()
        arg = " ".join(parts[1:]) if len(parts) > 1 else ""
        match cmd:
            case "help":
                self.ui.MainDisplayTextEdit.append("\nCommands: \n-start: start game\n-move #room_number: Move to another room\n-msg <name>:send a message to a player  \n-fight: Attack monsters in room \n-pvp <name>: initiate a fight against a player in the same room \n-loot <name>: Loot gold from dead character \n-help: Show this help \n to quit click the Quit Button (top right of the window) ")
                self.ui.MainDisplayTextEdit.moveCursor(QtGui.QTextCursor.End)
            case "start":
                packet=pack('<B',6)
                self.game_state.player_socket.send(packet)
            case "move":
                if len(parts)==1 or not parts[1].isdigit() or int(parts[1]) not in self.available_rooms:
                    #print(parts[1])
                    self.ui.MainDisplayTextEdit.append("to move to another room enter: move #room_number, available rooms:"+str(self.available_rooms))
                    self.ui.MainDisplayTextEdit.moveCursor(QtGui.QTextCursor.End)

                else:
                    packet=pack('<BH',2,int(parts[1]))
                    self.game_state.player_socket.send(packet)
                    self.ui.CharactersTextEdit.clear()
                    self.ui.AvailableRoomsTextEdit.clear()
                    self.other_characters.clear()
                    self.available_rooms.clear()
            case "msg":
                if len(parts) < 2:
                    self.ui.MainDisplayTextEdit.append("Usage: msg <recipient_name>")
                    self.ui.MainDisplayTextEdit.moveCursor(QtGui.QTextCursor.End)
                else:
                    recipient = parts[1]
                    # No message yet, prompt for it
                    self.current_msg_recipient = recipient  # Save recipient
                    self.ui.General_input.clear()
                    self.ui.InstructionTextEdit.setText(f"Enter message for {recipient}:")
                    
                    # Disconnect current handler and connect message input handler
                    try:
                        self.ui.sendButton.clicked.disconnect(self.handle_game_input)
                    except:
                        pass
                    self.ui.sendButton.clicked.connect(self.handle_message_input)
            case "fight":
                packet=pack('<B',3)
                self.game_state.player_socket.send(packet)
            case "loot":
                target_name = arg.strip()
                if not target_name or target_name not in self.other_characters.keys() or self.other_characters[target_name]['flags']['alive']:
                    self.ui.MainDisplayTextEdit.append("to loot another player enter: loot #target_player_name, lootable players:"+str([i['name'] for i in self.other_characters.values() if not i['flags']['alive']]))
                    self.ui.MainDisplayTextEdit.moveCursor(QtGui.QTextCursor.End)
                else:
                    name = target_name.encode("utf-8")
                    name = name.ljust(32, b'\x00')   # pad to 32 bytes

                    packet = pack('<B32s', 5, name)
                    self.game_state.player_socket.send(packet)
            case "pvp":
                if len(parts)==1 or str(parts[1]) not in self.other_characters.keys() or not self.other_characters[str(parts[1])]['flags']['alive']:
                    self.ui.MainDisplayTextEdit.append("to fight another player enter: pvp #target_player_name, \nfightable players:"+str([i['name'] for i in self.other_characters.values() if i['flags']['alive'] and not i['flags']['monster']]))
                    self.ui.MainDisplayTextEdit.moveCursor(QtGui.QTextCursor.End)
                else:
                    name = parts[1].encode("utf-8")
                    name = name.ljust(32, b'\x00')   # pad to 32 bytes

                    packet = pack('<B32s', 4, name)
                    self.game_state.player_socket.send(packet)
            case _:
                self.ui.MainDisplayTextEdit.append("\nCommands: \n-start: start game\n-move #room_number: Move to another room\n-msg <name>:send a message to a player  \n-fight: Attack monsters in room \n-pvp <name>: initiate a fight against a player in the same room \n-loot <name>: Loot gold from dead character \n-help: Show this help \n to quit click the Quit Button (top right of the window) ")
                self.ui.MainDisplayTextEdit.moveCursor(QtGui.QTextCursor.End)

                
                                    
    def send_leave(self):
        self.close()



        


#---------------this function will do everything(connect and allow people to play)---------------------------#
    

    def connect_and_load_game(self):
        #print("Function called!")
        port_text = self.ui.port_input.text()
        display_text=''
        
        if not port_text:
            self.ui.MainDisplayTextEdit.setText("Error: Please enter a port number.")
            return

        try:
            port = int(port_text)
            if not (5000 <= port <= 5200):
                self.ui.MainDisplayTextEdit.setText(f"Connection refused: no server listening on port {port}")
                return 
        except ValueError:
            self.ui.MainDisplayTextEdit.setText("Error: Port must be an integer.")
            return

        try:
            self.game_state.player_socket = socket(AF_INET, SOCK_STREAM)
            self.game_state.player_socket.connect(("isoptera.lcsc.edu", port))

            # Read first byte to check message type
            first_byte = self.game_state.player_socket.recv(1, 0)
            
            if first_byte[0] == 11:  # Lurk 2.0 - GAME message comes first
                game_version = "2.0"
                # Read rest of GAME message
                game_header_bytes = self.game_state.player_socket.recv(6, 0)
                initial_points, stat_limit, description_length = unpack("<HHH", game_header_bytes)
                game_description_bytes = self.game_state.player_socket.recv(description_length, 0)
                game_description = game_description_bytes.decode("utf-8", errors="replace")
            else:  # Lurk 2.3 - Version message comes first
                v = first_byte + self.game_state.player_socket.recv(4, 0)
                game_version = str(v[1])+'.'+str(v[2])
                
                game_header_bytes = self.game_state.player_socket.recv(7, 0)
                game_type, initial_points, stat_limit, description_length = unpack("<BHHH", game_header_bytes)
                game_description_bytes = self.game_state.player_socket.recv(description_length, 0)
                game_description = game_description_bytes.decode("utf-8", errors="replace")

            g = self.displayGame(game_version, initial_points, stat_limit, description_length, game_description, port)
            if g:
                t = self.get_player()

        #handling all kinds of errors to avoid crashing  the app
        except ConnectionRefusedError:
            self.ui.MainDisplayTextEdit.setText(f"Error: Connection refused on port {port}.")
        except TimeoutError:
            self.ui.MainDisplayTextEdit.setText(f"Error: Connection timed out on port {port}.")
        except OSError as e:
            self.ui.MainDisplayTextEdit.setText(f"Error: Cannot connect to port {port}. {e}")
        except Exception as e:
            self.ui.MainDisplayTextEdit.setText(f"Connection Error: {e}")


                

# --- Main Execution Block ---
if __name__ == '__main__':
    app = QtWidgets.QApplication(sys.argv)
    client_app = ClientApp()
    client_app.show()
    sys.exit(app.exec_())
