//////////////////////////////////////////////////////////////////
//
// Test για το state.h module
//
//////////////////////////////////////////////////////////////////

#include <stdlib.h>
#include <math.h>
#include <memory.h>
#include "acutest.h"			// Απλή βιβλιοθήκη για unit testing

#include "state.h"


///// Βοηθητικές συναρτήσεις ////////////////////////////////////////
//
// σύγκριση double
static bool double_equal(double a, double b) {
	return fabs(a-b) < 1e-6;
}

// αρχικοποίηση πλήκτρων (κανένα πατημένο)
static struct key_state empty_key_state() {
	struct key_state ks = { .space=false, .n=false };
	memset(ks.active_keys, 0, sizeof(ks.active_keys));
	memset(ks.changed_keys, -1, sizeof(ks.changed_keys));
	return ks;
}
/////////////////////////////////////////////////////////////////////


void test_state_create() {
	State state = state_create("test.mid");
	TEST_ASSERT(state != NULL);

	StateInfo info = state_info(state);
	TEST_ASSERT(info != NULL);
	TEST_ASSERT(info->paused);
	TEST_ASSERT(!info->game_over);
	TEST_ASSERT(double_equal(info->time, 0.0));
	TEST_ASSERT(info->level == 1);

	state_destroy(state);
}

void test_state_basic_functions() {
	State state = state_create("test.mid");
	TEST_ASSERT(state != NULL);

	TEST_ASSERT(state_playing_channel(state) == 9);
	TEST_ASSERT(double_equal(state_measure_duration(state), 1.93548));
	TEST_ASSERT(double_equal(state_total_duration(state), 18.987));

	state_destroy(state);
}


void test_state_update() {
	State state = state_create("test.mid");
	TEST_ASSERT(state != NULL);

	// Παράδειγμα:
	struct key_state ks = empty_key_state();

	ks.space = true;
	state_update(state, &ks, 0.0);
	TEST_ASSERT(!state_info(state)->paused);

	//έλεγχος αν το παιχνίδι προχωρά αν το παιχνίδι είναι ενεργό(δηλ. αυξάνεται κατά elapsed_time)
	ks.space=false;
	StateInfo info = state_info(state);
	double old=info->time;
	state_update(state, &ks, 0.5);
	TEST_ASSERT(double_equal(info->time,old+0.5));

	//το παιχνίδι πρέπει να γίνεται paused αν πατηθεί space
	ks.space = true;
	state_update(state, &ks, 0.0);
	TEST_ASSERT(state_info(state)->paused);

	//αν το παιχνίδι είναι paused και πατηθεί n ο χρόνος προχωρά frame by frame
	ks.n=true;
	old=info->time;
	state_update(state, &ks, 0.3);
	TEST_ASSERT(double_equal(info->time,old+0.3));


	//ΕΜΜΕΣΟΙ ΕΛΕΓΧΟΙ 

	//έλεγχος αν έγινε η αλλαγή πλήκτρου
	ks=empty_key_state();
	ks.changed_keys[4]=100;//velocity=100 σημαίνει note_on
	state_update(state, &ks, 0.3);

	//ελεγχος για ενεργό πλήκτρο
	ks.active_keys[60]=1;
	state_update(state, &ks, 1000.9);

	state_destroy(state);
}

void test_state_displayed_notes(){
	State state = state_create("test.mid");
	TEST_ASSERT(state != NULL);
	StateInfo info = state_info(state);
	double time_window=3.7839;
	List test_func=state_displayed_notes(state, time_window);
	TEST_ASSERT(test_func!= NULL);

	for(ListNode i=list_first(test_func);i!=LIST_EOF;i=list_next(test_func,i)){//παιρνω τις νότες που θα εμφανιστούν στην οθονη
		MidiEvent list_event=list_node_value(test_func, i);
		TEST_ASSERT(list_event->type==MIDI_NOTE);//ελεγχος αν είναι νότα αφού η συνάρτηση πρέπει να επιστρέφει μόνο νοτες
		TEST_ASSERT(list_event->time>=info->time);//έλεγχος αν βρίσκεται στο μέλλον(δεν θελουμε νότες απο το παρελθόν)
		TEST_ASSERT(list_event->time<=time_window+info->time);//έλεγχος αν βρίσκεται στο σωστό χρονικό διάστημα(το διάστημα που βλέπει-βρίσκεται ο χρήστης)
	}

	list_destroy(test_func);

	//έλεγχος για δεύτερο time_window
	time_window=1.033;
	test_func=state_displayed_notes(state, time_window);
	TEST_ASSERT(test_func!= NULL);

	for(ListNode i=list_first(test_func);i!=LIST_EOF;i=list_next(test_func,i)){
		MidiEvent list_event=list_node_value(test_func, i);
		TEST_ASSERT(list_event->type==MIDI_NOTE);
		TEST_ASSERT(list_event->time>=info->time);
		TEST_ASSERT(list_event->time<=time_window+info->time);	
	}

	list_destroy(test_func);

	state_destroy(state);
}

void test_state_playback_events(){
	State state = state_create("test.mid");
	TEST_ASSERT(state != NULL);
	StateInfo info = state_info(state);
	double since=2.103;


	List test_func=state_playback_events(state, since);
	TEST_ASSERT(test_func!= NULL);
	//παιρνω τα events του παρελθόντος(που έχιυν ήδη συμβεί)
	for(ListNode i=list_first(test_func);i!=LIST_EOF;i=list_next(test_func,i)){
		MidiEvent list_event=list_node_value(test_func, i);
		//έλεγχος αν βρίσκονται στο σωστό χρονικό διάστημα
		TEST_ASSERT(list_event->time>=info->time-since);//έλεγχος αν τα events βρίσκοται από το since
		TEST_ASSERT(list_event->time<=info->time);//έλεγχος αν το event βρίσκεται μέχρι το τωρα 
	}

	list_destroy(test_func);

	//έλεγχος για άλλο since
	since=0.543;
	test_func=state_playback_events(state, since);
	TEST_ASSERT(test_func!= NULL);

	for(ListNode i=list_first(test_func);i!=LIST_EOF;i=list_next(test_func,i)){
		MidiEvent list_event=list_node_value(test_func, i);
		TEST_ASSERT(list_event->time>=info->time-since);
		TEST_ASSERT(list_event->time<=info->time);
	}

	list_destroy(test_func);
	state_destroy(state);
}

void test_evaluate_clip_state_update(){//ελέγχει τι γίνεται αφού τελειώσει το τραγούδι
	State state = state_create("test.mid");
	TEST_ASSERT(state != NULL);
	StateInfo info = state_info(state);

	struct key_state ks = empty_key_state();
	//έλεγχος για game over σε περίπτωση που το accuracy δεν ειναι χαμηλό
	info->paused = false;
	info->accuracy = 0.0;
	info->game_over = false;
	state_update(state, &ks, state_total_duration(state) + 1.0);//προχωρώ τον χρόνο μετα το τέλος του τραγουδιού
	TEST_ASSERT(double_equal(info->score,0.0));
	TEST_ASSERT(double_equal(info->accuracy,0.0));
	
	TEST_ASSERT(info->game_over == true);//έλεγχος αν τελειωσε το παιχνίδι
	TEST_ASSERT(info->paused == true);//και μπηκε σε pause

	state_destroy(state);
	
	//έλεγχος αν ανεβαίνει level όταν ο παικτης έχει καλό accuracy
	state = state_create("test.mid");
	TEST_ASSERT(state != NULL);
	info = state_info(state);
	ks = empty_key_state();
	info->paused = false;
	info->accuracy = 1.0;
	info->game_over = false;

	int old_level=info->level;

	state_update(state, &ks, state_total_duration(state) + 1.0);
	//αν accuracy είναι ικανοποιητικό
	TEST_ASSERT(info->level==old_level+1);//level up
	TEST_ASSERT(double_equal(info->time,0.0));//restart χρόνου
	//το παιχνίδι δεν σταματά ούτε τελειωνει,συνεχίζει κανονικά
	TEST_ASSERT(info->game_over==false);
	TEST_ASSERT(info->paused==false);
	
	state_destroy(state);

}

// Λίστα με όλα τα tests προς εκτέλεση
TEST_LIST = {
	{ "test_state_create", test_state_create },
	{ "test_state_basic_functions", test_state_basic_functions },
	{ "test_state_update", test_state_update },
	{ "test_state_displayed_notes",test_state_displayed_notes},
	{ "test_state_playback_events",test_state_playback_events},
	{ "test_evaluate_clip_state_update",test_evaluate_clip_state_update},
	{ NULL, NULL } // τερματίζουμε τη λίστα με NULL
};
