char *vga = (char*) 0xB8000;
int cursor = 0;

int text_color = 0xF0;   
int status_color = 0x70; 

struct File {
    char name[32];
    char content[512];
    int used;
};

struct File files[10];

void scroll_screen() {
    int i = 0;
    while (i < 80 * 24) {
        vga[i * 2] = vga[(i + 80) * 2];
        vga[i * 2 + 1] = vga[(i + 80) * 2 + 1];
        i = i + 1;
    }
    i = 80 * 24;
    while (i < 80 * 25) {
        vga[i * 2] = ' ';
        vga[i * 2 + 1] = text_color;
        i = i + 1;
    }
    cursor = cursor - 80;
}

void putc(char c) {
    if (c == '\n') {
        int row = cursor / 80;
        cursor = (row + 1) * 80;
    } else if (c == '\b') {
        if (cursor > 0) {
            cursor = cursor - 1;
            vga[cursor * 2] = ' ';
            vga[cursor * 2 + 1] = text_color; 
        }
    } else {
        vga[cursor * 2] = c;
        vga[cursor * 2 + 1] = text_color; 
        cursor = cursor + 1;
    }
    
    if (cursor >= 80 * 25) {
        scroll_screen();
    }
}

void write(char *str) {
    int i = 0;
    while (str[i] != 0) {
        putc(str[i]);
        i = i + 1;
    }
}

void writeline(char *str) {
    write(str);
    putc('\n');
}

void clear_screen() {
    int i = 0;
    while (i < 80 * 25) {
        vga[i * 2] = ' ';
        vga[i * 2 + 1] = text_color; 
        i = i + 1;
    }
    cursor = 0;
}

int my_strcmp(char *a, char *b) {
    int i = 0;
    while (a[i] != 0 && b[i] != 0) {
        if (a[i] != b[i]) {
            return 1;
        }
        i = i + 1;
    }
    if (a[i] == 0 && b[i] == 0) {
        return 0;
    }
    return 1;
}

unsigned char inb(unsigned short port) {
    unsigned char result;
    __asm__ volatile("inb %1, %0" : "=a"(result) : "Nd"(port));
    return result;
}

char kbd_map[128] = {
    0,  27, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b',
    '\t', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',
    0,  'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`',
    0, '\\', 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', 0, '*', 0, ' '
};

char kbd_map_shift[128] = {
    0,  27, '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+', '\b',
    '\t', 'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', '\n',
    0,  'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', '"', '~',
    0, '|', 'Z', 'X', 'C', 'V', 'B', 'N', 'M', '<', '>', '?', 0, '*', 0, ' '
};

char editor[24][80];
int ed_x = 0;
int ed_y = 0;
int ed_mode = 0;
int current_file = -1;
int ext_key = 0;
int shift_pressed = 0;

void redraw_editor() {
    int x = 0;
    int y = 0;
    
    clear_screen();
    
    while (y < 24) {
        x = 0;
        while (x < 80) {
            char c = editor[y][x];
            if (c != 0) {
                vga[(y * 80 + x) * 2] = c;
                vga[(y * 80 + x) * 2 + 1] = text_color;
            }
            x = x + 1;
        }
        y = y + 1;
    }
    
    x = 0;
    while (x < 80) {
        vga[(24 * 80 + x) * 2] = ' ';
        vga[(24 * 80 + x) * 2 + 1] = status_color;
        x = x + 1;
    }
    
    char *title = " LogOS Editor | Esc=Save ";
    x = 0;
    while (title[x] != 0) {
        vga[(24 * 80 + x) * 2] = title[x];
        vga[(24 * 80 + x) * 2 + 1] = status_color;
        x = x + 1;
    }
    
    unsigned short pos = (ed_y * 80) + ed_x;
    
    __asm__ volatile("outb %0, %1" : : "a"((unsigned char)0x0F), "Nd"((unsigned short)0x3D4));
    __asm__ volatile("outb %0, %1" : : "a"((unsigned char)(pos & 0xFF)), "Nd"((unsigned short)0x3D5));
    __asm__ volatile("outb %0, %1" : : "a"((unsigned char)0x0E), "Nd"((unsigned short)0x3D4));
    __asm__ volatile("outb %0, %1" : : "a"((unsigned char)((pos >> 8) & 0xFF)), "Nd"((unsigned short)0x3D5));
}

void lungs_main() {
    clear_screen();
    writeline("LogOS v1.6 (Cat Fix)");
    writeline("Type 'help' for commands.");
    write("> ");

    unsigned char last_scancode = 0;
    char cmd_buf[128];
    int cmd_len = 0;

    while (1) {
        unsigned char scancode = inb(0x60);
        
        if (scancode != last_scancode) {
            last_scancode = scancode;

            if (scancode == 0xE0) { 
                ext_key = 1; 
                continue; 
            }

            if (scancode == 0x2A || scancode == 0x36) {
                shift_pressed = 1;
                continue;
            }
            
            if (scancode == 0xAA || scancode == 0xB6) {
                shift_pressed = 0;
                continue;
            }

            if (scancode < 0x80) {
                
                if (ed_mode == 1) {
                    
                    if (scancode == 0x01) {
                        int last_y = 0;
                        int ty = 0;
                        int tx = 0;
                        while (ty < 24) {
                            tx = 0;
                            while (tx < 80) {
                                if (editor[ty][tx] != 0) {
                                    last_y = ty;
                                }
                                tx = tx + 1;
                            }
                            ty = ty + 1;
                        }

                        int i = 0;
                        int y = 0;
                        int x = 0;
                        while (y <= last_y) {
                            x = 0;
                            while (x < 80) {
                                if (editor[y][x] != 0) {
                                    files[current_file].content[i] = editor[y][x];
                                    i = i + 1;
                                }
                                x = x + 1;
                            }
                            files[current_file].content[i] = '\n';
                            i = i + 1;
                            y = y + 1;
                        }
                        files[current_file].content[i] = 0;
                        
                        ed_mode = 0;
                        clear_screen();
                        write("> ");
                        cmd_len = 0;
                        continue;
                    }

                    if (ext_key == 1) {
                        if (scancode == 0x48) { if (ed_y > 0) ed_y = ed_y - 1; }
                        if (scancode == 0x50) { if (ed_y < 23) ed_y = ed_y + 1; }
                        if (scancode == 0x4B) { if (ed_x > 0) ed_x = ed_x - 1; }
                        if (scancode == 0x4D) { if (ed_x < 79) ed_x = ed_x + 1; }
                        redraw_editor();
                        ext_key = 0;
                        continue;
                    }
                    
                    char c;
                    if (shift_pressed == 1) { c = kbd_map_shift[scancode]; } 
                    else { c = kbd_map[scancode]; }
                    
                    if (c == '\b') {
                        if (ed_x > 0) {
                            ed_x = ed_x - 1;
                            int i = ed_x;
                            while (i < 79) {
                                editor[ed_y][i] = editor[ed_y][i + 1];
                                if (editor[ed_y][i] == 0) { break; }
                                i = i + 1;
                            }
                        }
                        redraw_editor();
                    } else if (c == '\n') {
                        ed_y = ed_y + 1;
                        ed_x = 0;
                        if (ed_y > 23) { ed_y = 23; }
                        redraw_editor();
                    } else if (c != 0) {
                        if (ed_x < 79) {
                            int len = 0;
                            while (len < 79 && editor[ed_y][len] != 0) { len = len + 1; }
                            int i = len;
                            while (i > ed_x) {
                                editor[ed_y][i] = editor[ed_y][i - 1];
                                i = i - 1;
                            }
                            editor[ed_y][ed_x] = c;
                            ed_x = ed_x + 1;
                        }
                        redraw_editor();
                    }
                    
                    ext_key = 0;
                    continue;
                }

                char c;
                if (shift_pressed == 1) { c = kbd_map_shift[scancode]; } 
                else { c = kbd_map[scancode]; }
                
                if (c != 0 && ext_key == 0) {
                    if (c == '\n') {
                        cmd_buf[cmd_len] = 0;
                        putc('\n');
                        
                        if (cmd_len > 0) {
                            if (my_strcmp(cmd_buf, "help") == 0) {
                                writeline("Commands: help, clear, ls, new, cat, edit, AboutOS");
                            } 
                            else if (my_strcmp(cmd_buf, "AboutOS") == 0) {
                                clear_screen();
                                writeline("##      ###   ####   ###   ####  ");
                                writeline("##     ## ##  ##    ## ##  ##    ");
                                writeline("##     ## ##  ## ## ## ##   ###  ");
                                writeline("##     ## ##  ##  # ## ##     ## ");
                                writeline("#####   ###    ###   ###  ####   ");
                                writeline("-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-");
                                writeline("Developer: most conscious");
                                writeline("File System: RAM");
                                writeline("what I will add in the future:");
                                writeline("1. a file system not in RAM");
                                writeline("2. I will add new terminal themes.");
                                writeline("3. new commands");
                                writeline("4. Transition to 64-bit (not anytime soon)");
                                writeline("5. support for launching programs (possibly not anytime soon)");
                                writeline("whats new right now:");
                                writeline("1. file system in RAM");
                                writeline("2. most basic file operations");
                                writeline("3. transition from 16 bits to 32 bits");
                                writeline("4. transition to C");
                                writeline("-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-");
                                writeline("");
                            }
                            else if (my_strcmp(cmd_buf, "clear") == 0) {
                                clear_screen();
                            } 
                            else if (my_strcmp(cmd_buf, "ls") == 0) {
                                int found = 0;
                                int i = 0;
                                while (i < 10) {
                                    if (files[i].used == 1) { writeline(files[i].name); found = 1; }
                                    i = i + 1;
                                }
                                if (found == 0) { writeline("(empty)"); }
                            } 
                            else if (cmd_buf[0] == 'n' && cmd_buf[1] == 'e' && cmd_buf[2] == 'w' && cmd_buf[3] == ' ') {
                                int created = 0;
                                int i = 0;
                                while (i < 10) {
                                    if (files[i].used == 0) {
                                        files[i].used = 1;
                                        int j = 0;
                                        while (cmd_buf[4 + j] != 0 && j < 31) { 
                                            files[i].name[j] = cmd_buf[4 + j]; 
                                            j = j + 1; 
                                        }
                                        files[i].name[j] = 0;
                                        files[i].content[0] = 0;
                                        writeline("File created!"); 
                                        created = 1; 
                                        break; 
                                    }
                                    i = i + 1;
                                }
                                if (created == 0) { writeline("Disk full!"); }
                            } 
                            else if (cmd_buf[0] == 'c' && cmd_buf[1] == 'a' && cmd_buf[2] == 't' && cmd_buf[3] == ' ') {
                                int found = 0;
                                int i = 0;
                                while (i < 10) {
                                    if (files[i].used == 1) {
                                        if (my_strcmp(files[i].name, cmd_buf + 4) == 0) {
                                            writeline(files[i].content); 
                                            found = 1; 
                                            break;
                                        }
                                    }
                                    i = i + 1;
                                }
                                if (found == 0) { writeline("Not found!"); }
                            } 
                            else if (cmd_buf[0] == 'e' && cmd_buf[1] == 'd' && cmd_buf[2] == 'i' && cmd_buf[3] == 't' && cmd_buf[4] == ' ') {
                                int found = 0;
                                int i = 0;
                                while (i < 10) {
                                    if (files[i].used == 1) {
                                        if (my_strcmp(files[i].name, cmd_buf + 5) == 0) {
                                            current_file = i;
                                            int y = 0;
                                            int x = 0;
                                            while (y < 24) {
                                                x = 0;
                                                while (x < 80) {
                                                    editor[y][x] = 0;
                                                    x = x + 1;
                                                }
                                                y = y + 1;
                                            }
                                            int idx = 0;
                                            y = 0;
                                            while (y < 24) {
                                                x = 0;
                                                while (x < 80) {
                                                    if (files[i].content[idx] == 0) { break; }
                                                    if (files[i].content[idx] == '\n') { idx = idx + 1; break; }
                                                    editor[y][x] = files[i].content[idx];
                                                    idx = idx + 1;
                                                    x = x + 1;
                                                }
                                                y = y + 1;
                                            }
                                            ed_x = 0; 
                                            ed_y = 0;
                                            ed_mode = 1;
                                            redraw_editor();
                                            found = 1; 
                                            break;
                                        }
                                    }
                                    i = i + 1;
                                }
                                if (found == 0) { writeline("Not found!"); }
                            } 
                            else {
                                writeline("Unknown command");
                            }
                        }
                        if (ed_mode == 0) { 
                            cmd_len = 0; 
                            write("> "); 
                        }
                    } 
                    else if (c == '\b') {
                        if (cmd_len > 0) { cmd_len = cmd_len - 1; putc('\b'); }
                    } 
                    else {
                        if (cmd_len < 127) { cmd_buf[cmd_len] = c; cmd_len = cmd_len + 1; putc(c); }
                    }
                }
            }
            ext_key = 0;
        }
    }
}
