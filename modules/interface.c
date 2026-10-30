#include "raylib.h"

#include "state.h"
#include "interface.h"

void interface_init() {
	// Αρχικοποίηση του παραθύρου
	InitWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "my_piano_game");
	SetTargetFPS(60);//το παιχνίδι θα ανανεώεται κάθε 60 φορές του δευτερόλεπτο
    InitAudioDevice();//αρχικοποιηση του συστήματος ήχου

}

void interface_close() {
	CloseAudioDevice();//κλείσιμο συστήματος ήχου
	CloseWindow();//κλεισιμο παραθύρου
}

// Draw game (one frame)
void interface_draw_frame(State state) {
	BeginDrawing();

	// Καθαρισμός, θα τα σχεδιάσουμε όλα από την αρχή, με φόντο darkgrey
	ClearBackground(DARKGRAY);

    int keys=30;// το πιάνο θα έχει 30 πλήκτρα
    int width_piano=SCREEN_WIDTH /keys;// που θα υπάρχουν σε όλο το κάτω μέρος της οθόνης
    int height_piano=SCREEN_HEIGHT /6;//το ύψος τους θα είναι ίσο με το 1/6 της οθόνης
    for(int i=0;i<keys;i++)//εδώ σχεδιάζονται τα πλήκτρα
        DrawRectangle(i* width_piano, SCREEN_HEIGHT-height_piano, width_piano,height_piano, WHITE); 

    StateInfo info = state_info(state);//παίρνω την κατάσταση του παιχνιδιού (για κάθε τωρινή στιγμή)

    // Σχεδιάζουμε το σκορ ,το accuracy και το FPS counter
    DrawText(TextFormat("%.2f",info->score), 20, 20, 20, WHITE);
    DrawText(TextFormat("%.2f",info->accuracy), 20, 50, 20, WHITE);
    DrawFPS(SCREEN_WIDTH - 80, 0);

	double time_window=2.0;//το χρονικό παράθυρο για το οποίο θα εμφανιστούν συγκεκριμένες νότες
    List notes=state_displayed_notes(state,time_window);
    for(ListNode j=list_first(notes);j!=LIST_EOF;j=list_next(notes,j)){//παιρνω κάθε νότα απο την λίστα ώστε να εμανιστεί αν όμως είναι ενεργή
        MidiEvent evnt=list_node_value(notes,j);

		if(evnt->velocity==0)
			continue;

        int key=evnt->key%30;//βρίσκω σε ποιο απο τα 30 πλήκτρα θα "πέσει"
		double time=evnt->time-info->time;//και βρίσκω πόσο απέχει απο την ώρα που πρέπει να παιχτεί
		double y=(SCREEN_HEIGHT-height_piano)-(time/time_window)*(SCREEN_HEIGHT-height_piano);
		double x=width_piano*key;
    }

	list_destroy(notes);

    	// Αν το παιχνίδι έχει τελειώσει, σχεδιάζουμε το μήνυμα για να ξαναρχίσει
	if (info->game_over) {
		DrawText(
			"PRESS [ENTER] TO PLAY AGAIN",
			 GetScreenWidth() / 2 - MeasureText("PRESS [ENTER] TO PLAY AGAIN", 20) / 2,
			 GetScreenHeight() / 2 - 50, 20, GRAY
		);
	}

    else if (info->paused) {
		DrawText(
			"PRESS [ENTER] TO RESUME",
			 GetScreenWidth() / 2 - MeasureText("PRESS [ENTER] TO RESUME", 20) / 2,
			 GetScreenHeight() / 2 - 50, 20, GRAY
		);
	}

    EndDrawing();
}   