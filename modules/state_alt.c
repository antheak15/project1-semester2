#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h> // //
#include "ADTList.h"
#include "ADTVector.h"
#include "k08midi.h"
#include "state.h"


// Οι ολοκληρωμένες πληροφορίες της κατάστασης του παιχνιδιού.
// Ο τύπος State είναι pointer σε αυτό το struct, αλλά το ίδιο το struct
// δεν είναι ορατό στον χρήστη.

struct state {
	struct state_info info;  // Γενικές πληροφορίες για την κατάσταση του παιχνιδιού
	MidiFile midi_file;      // Το .mid αρχείο που διαβάσαμε με τη k08midi_file_read
	List midi_events;        // MIDI events προς αναπαραγωγή
	Vector clips;            // Clip προς ηχογράφηση
	uint recording_index;    // index του τρέχοντος (ή επόμενου) clip
    Vector song_events; //κάθε θέση του vector θα δείχνει σε ενα midi event //
	uint current_index;//δείκτης στο πρώτο event πιο κοντά στο time
	uint playback_index;//δείκτης στο πρώτο event πιο κοντά στο time-since

// Τμήμα του τραγουδιού στο οποίο υπάρχει marker "rec" και ο χρήστης πρέπει να
// ηχογραφήσει, παίζοντας τις νότες από το αντίστοιχο channel του τραγουδιού. Το
// ηχογραφημένο τμήμα αναπαράγεται σε loop στα σημεία που υπάρχουν οι
// αντίστοιχοι "play" markers.
};
typedef struct clip {
	String name;		// όνομα
	int channel;		// MIDI κανάλι
	double start;		// χρονικό σημείο έναρξης
	double duration;	// διάρκεια
	List recorded;  	// Ηχογραφημένα MIDI_NOTE events
	List plays;     	// Λίστα με τα σημεία αναπαραγωγής του clip (τύπος ClipPlay)
}* Clip;

// Τμήμα αναπαραγωγής ενός clip, που προκύπτει από τα "play" markers.

typedef struct clip_play {
	double start;		// χρονικό σημείο έναρξης
	double duration;	// διάρκεια
}* ClipPlay;


// Bοηθητικές συναρτήσεις /////////////////////////////////////////////////////////////////////////////////
//
// Δημιουργεί και επιστρέφει ένα σημείο αναπαραγωγής clip
static ClipPlay create_clip_play(double start, double duration) {
	ClipPlay play = malloc(sizeof(*play));
	play->start = start;
	play->duration = duration;
	return play;
}

// Δημιουργεί και επιστρέφει ένα clip
static Clip create_clip(State state, String name, int channel, double start, double duration) {
	Clip clip = malloc(sizeof(*clip));
	clip->name = strdup(name);
	clip->channel = channel;
	clip->start = start;
	clip->duration = duration;
	clip->recorded = list_create(free);
	clip->plays = list_create(free);
	return clip;
}

// Καταστρέφει ένα clip
static void destroy_clip(Pointer value) {
	Clip clip = value;
	free(clip->name);
	list_destroy(clip->recorded);
	list_destroy(clip->plays);
	free(clip);
}

// Βρίσκει και επιστρέφει ένα clip με βάση το όνομά του, NULL αν δεν υπάρχει.
static Clip find_clip_by_name(State state, String clip_name) {
	for (int ci = 0; ci < vector_size(state->clips); ci++) {
		Clip clip = vector_get_at(state->clips, ci);
		if (strcmp(clip->name, clip_name) == 0)
			return clip;
	}
	return NULL;
}

int max(int a,int b){
	if(a>b)
		return a;
	else 
		return b;
}

double required_accuracy(State state){
	return 0.7;//για να είναι το accuracy ικανοποιητικό πρέπει ο παίκτης να πετύχει το 70%//
}
// Αντιγράφει στο state->midi_events όλα τα PROGRAM_CHANGE (αλλαγή οργάνου) και
// CONTROL_CHANGE (έλεγχος volume, modulation, sustain, κλπ), ώστε να είναι
// έτοιμα για αναπαραγωγή μαζί με τις ηχογραφημένες νότες. 
static void append_song_control_events(State state) {
	for (ListNode node = list_first(state->midi_file->events); node != LIST_EOF; node = list_next(state->midi_file->events, node)) {
		MidiEvent event = list_node_value(state->midi_file->events, node);
		if (event->type == MIDI_CONTROL_CHANGE || event->type == MIDI_PROGRAM_CHANGE) {
			MidiEvent clone = malloc(sizeof(*clone));
			*clone = *event; // clone
			list_insert_next(state->midi_events, list_last(state->midi_events), clone);
		}
	}
}

// Δημιουργεί clips και clip_plays με βάση τα markers του αρχείου MIDI
static void create_clips(State state) {
	state->clips = vector_create(0, destroy_clip);
	double measure_duration = state_measure_duration(state);

	for (ListNode node = list_first(state->midi_file->events); node != LIST_EOF; node = list_next(state->midi_file->events, node)) {
		MidiEvent event = list_node_value(state->midi_file->events, node);
		if (event->type != MIDI_MARKER)
			continue;

		int channel = 0;
		int measures = 0;
		char clip_name[51]; // στη sscanf διαβάζουμε το πολύ 50 χαρακτήρες για όνομα

		// rec και play markers
		if (sscanf(event->marker, "rec,%50[^,],%d,%d", clip_name, &channel, &measures) == 3) {
			// "rec,<name>,<channel>,<measures>" marker, ορίζει ένα clip προς ηχογράφηση
			Clip clip = create_clip(state, clip_name, channel, event->time, measures * measure_duration);
			vector_insert_last(state->clips, clip);

		} else if (sscanf(event->marker, "play,%50[^,],%d", clip_name, &measures) == 2) {
			// "play,<name>,<measures>" marker, ορίζει ένα σημείο αναπαραγωγής του clip
			Clip clip = find_clip_by_name(state, clip_name);
			assert(clip);	// το .mid αρχείο πρέπει να έχει φτιαχτεί σωστά, για κάθε play να προηγείται ένα rec
			list_insert_next(clip->plays, list_last(clip->plays), create_clip_play(event->time, measures * measure_duration));
		}
	}
}
/////////////////////////////////////////////////////////////////////////////////////////////////////

static void state_create_helper(State state){//βοηθητική συνάρτηση που μετατρέπει την λίστα σε vector ώστε να εχουμε ποιο γρήγορη πρόσβαση
	state->song_events=vector_create(0, NULL);//δημιουργώ κενό vector ώστε κάθε φορά που μετατρέπεται ένας κόμβος να αυξάνεται και το size
	for(ListNode i=list_first(state->midi_file->events);i!=LIST_EOF;i=list_next(state->midi_file->events,i)){//πέρνω ένα κόμβο κάθε φορά
		MidiEvent evnt=list_node_value(state->midi_file->events, i);//πέρνω το event ώστε να το προσθέσω στο vector
		vector_insert_last(state->song_events, evnt);
	}
}
State state_create(String midi_file) {
	State state = malloc(sizeof(*state));
	assert(state != NULL);

	state->info.paused = true;
	state->info.game_over = false;
	state->info.accuracy = 0.0;
	state->info.score = 0.0;
	state->info.time = 0.0;
	state->info.level = 1;

	state->midi_file = NULL;
	state->midi_events = list_create(free);

	state->midi_file = k08midi_file_read(midi_file);
	assert(state->midi_file != NULL);
	state_create_helper(state);
	
	create_clips(state);
	assert(vector_size(state->clips) > 0);
	state->recording_index = 0;
	state->current_index=0;
	state->playback_index=0;
	list_destroy(state->midi_events);
	state->midi_events = list_create(free);
	append_song_control_events(state);

	return state;
}

StateInfo state_info(State state) {//η συνάρτηση είναι τύπου StateInfo(pointer του struct state_info)
	return &state->info; //το info του state ειναι struct state_info άρα επιστρέφεται η διεύθυνση του info
}

int state_playing_channel(State state) {
	// Το state->recording_index ξεκινάει από το 0 και δείχνει πάντα είτε στο
	// clip που ηχογραφείται τώρα, είτε (αν δεν υπάρχει ενεργή ηχογράφηση) στο
	// αμέσως επόμενο clip. Αφού ολοκληρωθούν όλες οι ηχογραφήσεις το
	// recording_index συνεχίζει να δείχνει το τελευταίο clip.
	//
	// Το κανάλι στο οποίο παίζει "live" ο χρήστης είναι το κανάλι του clip στο
	// οποίο δείχνει το recording_index.

	//βρίσκω το συγκεκριμένο index το clip που υπάρχει
	Clip clp=vector_get_at(state->clips,state->recording_index);

	return clp->channel; // στο clp βρίσκεται το channel που θέλω να επιστραφεί
}

double state_measure_duration(State state) {
	MidiFile midi = state->midi_file;
	return midi->time_signature[0] * (60.0 / midi->tempo) * (4.0 / midi->time_signature[1]);
}

double state_total_duration(State state) {
	return state->midi_file->duration; //χρησιμοποιώ το midi_file και οχι το midi_event αφού θελω την διάρκεια όλου του τραγουδιού
}

List state_displayed_notes(State state, double time_window) {
	List list=list_create(NULL);//η λίστα που θα επιστραφεί//

	while(state->current_index<vector_size(state->song_events)){//προχωρώ το current_index μέχρι να δειχνεί στο πρώτο event στον ζητούμενο χρόνο
		MidiEvent evnt=vector_get_at(state->song_events,state->current_index );

		if(evnt->time>=state->info.time)
			break;//αν ξεπεράσουμε το χρονο σημαίνει βρέθηκε άρα σταματα το loop 
		state->current_index++;
	}

	for(int i=state->current_index;i<vector_size(state->song_events);i++){//ξεκινώ από το πρώτο event που δεν είναι πριν τον χρόνο που θέλουμε
		MidiEvent evnt=vector_get_at(state->song_events, i);
		//ελέγχω αν το event είναι νότα και βρίσκεται σε χρονικό διάστημα που μπορεί να παίξε ο χρήστης//
		if((evnt->type==MIDI_NOTE)&&(evnt->time>=state->info.time)&&(evnt->time<=(time_window+state->info.time))){
			list_insert_next(list,list_last(list), evnt);
		}
		if(evnt->time>state->info.time+time_window)
			break;// αν ξεπεράσουμε το χρόνο η διαδικασία τελειώνει, δεν χρειάζεται να συνεχίσω
	}
	return list;
}

List state_playback_events(State state, double since) {
	//κάνω παρόμοια διαδικασία με την  state_displayed_notes αλλά για όλο διάστημα χρόνου
	List list=list_create(NULL);//η  λίστα που θα περιέχει τα events απο το παρελθον//

	while(state->playback_index<vector_size(state->song_events)){
		MidiEvent evnt=vector_get_at(state->song_events,state->playback_index );

		if(evnt->time>=state->info.time-since)
			break;
		state->playback_index++;
	}

	for(int i=state->playback_index;i<vector_size(state->song_events);i++){
		MidiEvent evnt=vector_get_at(state->song_events,i);
		if(evnt->time<state->info.time-since)
			continue;
		if((evnt->time>=state->info.time-since)&&(evnt->time<=state->info.time)){//ελέγχω αν βρισκόταν στο παρελθον//
			list_insert_next(list,list_last(list), evnt);//την προσθέτο μετά απο τον τελευταίο κόμβο//
		}
		if(evnt->time>state->info.time)
			break;
		
	}
	return list;
}

static void evaluate_clip(State state,Clip clp){
	List clip_song=list_create(NULL);
	double total_score=0,clip_accuracy=0;
	int song_note_on_count=0;
	int played_note_on_count=0;
	int check=0;
	for(ListNode i=list_first(state->midi_file->events);i!=LIST_EOF;i=list_next(state->midi_file->events,i)){

		MidiEvent evnt=list_node_value(state->midi_file->events, i);

		if((evnt->time>=clp->start)&&(evnt->time<clp->start+clp->duration)){
			list_insert_next(clip_song,list_last(clip_song), evnt);

		if(evnt->type==MIDI_NOTE && evnt->velocity>0)
			song_note_on_count++;
		}
	}

	for(ListNode i=list_first(clp->recorded);i!=LIST_EOF;i=list_next(clp->recorded,i)){

		MidiEvent note1=list_node_value(clp->recorded, i);
		MidiEvent match=NULL;
		ListNode prev=LIST_BOF,func_pos=LIST_EOF;

		if(note1->type==MIDI_NOTE && note1->velocity>0){
			played_note_on_count++;

			for(ListNode j=list_first(clip_song);j!=LIST_EOF;j=list_next(clip_song,j)){
				MidiEvent note2=list_node_value(clip_song, j);

				if(note2->type==MIDI_NOTE && note2->velocity>0){
					if(note1->key==note2->key && fabs(note1->time-note2->time)<=0.1){
						match=note2;
						func_pos=prev;
						break;
					}
				}
				prev=j;
			}

			if(match!=NULL){ //σε περίπτωση που δεν ύπαρχει match δεν προχωρά η διαδικασία//
				check=1;
				MidiEvent off_note2=NULL;
				MidiEvent off_note1=NULL;

				for(ListNode k=list_first(clp->recorded);k!=LIST_EOF;k=list_next(clp->recorded,k)){
					MidiEvent off1=list_node_value(clp->recorded, k);
					if(off1->type==MIDI_NOTE && off1->velocity==0 && off1->key==note1->key &&  off1->time>note1->time ){
						off_note1=off1;
						break;
					}
					
					}
					for(ListNode n=list_first(clip_song);n!=LIST_EOF;n=list_next(clip_song,n)){
						MidiEvent off2=list_node_value(clip_song, n);
						if(off2->type==MIDI_NOTE && off2->velocity==0 && off2->key==match->key &&  off2->time>match->time ){
							off_note2=off2;
							break;
						}
					}

			if(off_note1!=NULL && off_note2!=NULL){
				double duration_note1=off_note1->time-note1->time;
				double duration_note2=off_note2->time-match->time;
				double duration_diff=fabs(duration_note1-duration_note2);
				double time_diff=fabs(match->time-note1->time);

				if(note1->channel==9)
					duration_diff=0;

				double diff=time_diff+duration_diff;
				double score = exp(-pow(diff / 0.1, 2));
				total_score+=score;
				note1->time=match->time;
				off_note1->time=off_note2->time;
				}

				list_remove_next(clip_song, func_pos);
			}
		}	
		
	}
	
	state->info.score+=total_score;
	if(check==0){
		clip_accuracy=0;
	}
		
	else{
		clip_accuracy=total_score/max(song_note_on_count, played_note_on_count);
	}

	state->info.accuracy=(state->info.accuracy*(state->recording_index +1)+clip_accuracy)/(state->recording_index+1);
	list_destroy(clip_song);
}

void state_update(State state, KeyState ks, double elapsed_time) {
	double old_time=state->info.time;//ελεγχος για το αν τέλειωσε το clip//
	if(ks->space==true){//αν ο χρήστης πάτησε το space//
		if((state->info.paused==false)&&(state->info.game_over==false))//αν το παιχνίδι είναι ενεργό με το πάτημα του space κάνει pause αλλά δεν τελειώνει//
			state->info.paused=true;
		
		else {// αλλίως βγαίνει απο pause,game over//
			state->info.paused=false;
			state->info.game_over=false;
		}
	}

	if ((state->info.paused==false)&&(state->info.game_over==false))//βασική λειτουργεία χρόνου απο οδηγεία//
		state->info.time+=elapsed_time;
	
	else if((state->info.paused==true)&&(ks->n==true)&&(state->info.game_over==false)){
		state->info.time+= elapsed_time;  //αν το παιχνίδι είναι paused και πατηθεί n ο χρόνος αυξανεται κατα frame-elapsed time//
	}

	Clip available_clip=vector_get_at(state->clips,state->recording_index);//παίρνω το τρέχον clip απο τα clips//
	if(state->info.time<available_clip->duration){// αν το clip είναι ενεργό//
		for(int i=0;i<128;i++){
			if(ks->changed_keys[i]>=0){//αν άλλαξε μόλις//
				MidiEvent temp=malloc(sizeof(*temp));//δεσμέυω μνήμη για ένα νεο event που περιέχει τις πληροφορίες του ενεργού clip//
				temp->time=state->info.time;
				temp->channel=available_clip->channel;
				temp->type=MIDI_NOTE;
				temp->key=i;
				temp->velocity=ks->changed_keys[i];

				list_insert_next(available_clip->recorded, list_last(available_clip->recorded), temp);// προσθέτω το νέο event στο τέλος της λίστας recorded//
			}

		}
	}
	//οταν ολοκληρωθεί το recording ενός clip//
	else if(old_time<available_clip->duration && state->info.time>=available_clip->duration){//αν το clip που βρισκόμουν πριν τελειωσε//
		for(int i=0;i<128;i++){//διαδικασία ώστε να απενεργωποιηθουν οι ενεργές νότες//
			if(ks->active_keys[i]>0){//αν >0 τότε είναι ακομα ενεργή//
				MidiEvent temp=malloc(sizeof(*temp));
				temp->time=available_clip->duration;
				temp->channel=available_clip->channel;
				temp->type=MIDI_NOTE;
				temp->key=i;
				temp->velocity=0;// προσθέτω note_off στην ενεργή νότα ενώ τα υπόλοιπα μένουν ίδια//

				list_insert_next(available_clip->recorded, list_last(available_clip->recorded), temp);
			}
		}

		evaluate_clip(state,available_clip);

		if(state->recording_index<vector_size(state->clips)-1){// αν δεν είμαι στη τελευταίο clip το index αυξάνεται//
			state->recording_index++;
		}

	}

	if(old_time<available_clip->duration && state->info.time>=available_clip->duration){//οταν ολοκληρωθεί το clip//
		for(ListNode i=list_first(available_clip->plays);i!=LIST_EOF;i=list_next(available_clip->plays,i)){//πάιρνω κάθε σημειο αναπαραγωγής του clip//
			ClipPlay cp=list_node_value(available_clip->plays, i);

			for(double j=0;j<cp->duration;j+=available_clip->duration){//επαναλαμβάνεται όσες φορές χρειαστεί το recorded clip ώστε να καλυφθεί η διάρκεια του play//

				for(ListNode k=list_first(available_clip->recorded);k!=LIST_EOF;k=list_next(available_clip->recorded,k)){//παιρνω κάθε event που ηχογραφηθηκε στο clip//

					MidiEvent old_event=list_node_value(available_clip->recorded, k);
					
					double temp =cp->start+j+old_event->time;//νέος χρόνος event//
					if(temp>=cp->duration+cp->start){
						continue;//αν παιρνά το όριο συνεχίζω με το επόμενο//
					}

					MidiEvent new_event=malloc(sizeof(*new_event));//δημιουργώ το νέο event αλλάζοντας μόνο το χρόνο//
					new_event->channel=old_event->channel;
					new_event->type=old_event->type;
					new_event->key=old_event->key;
					new_event->velocity=old_event->velocity;
					new_event->time=temp;

					list_insert_next(state->midi_events, list_last(state->midi_events), new_event);//το προσθέτο στη λίστα midi_events του state//

				}
			}
		}
	}

	if(old_time<state->midi_file->duration && state->info.time>=state->midi_file->duration){
		if(state->info.accuracy>=required_accuracy(state)){
			state->info.level++;
			state->info.time=0;
			state->info.game_over=false;
			state->info.paused=false;
			state->recording_index=0;
			state->current_index=0;//το τραγούδι ξεκινά απο την αρχή άρα επαναφέρω τους δείκτες
			state->playback_index=0;

		}
		else{
			state->info.game_over=true;
			state->info.paused=true;
		}
	}


}
void state_destroy(State state) {
	if(state==NULL)
		return;
	list_destroy(state->midi_events);
	k08midi_file_destroy(state->midi_file);
	vector_destroy(state->clips);
	vector_destroy(state->song_events);//καταστρέφω το vector song_events που δημιουργησα
	 free (state);
}