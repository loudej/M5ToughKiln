#ifndef PROGRAM_SELECTION_SNAPSHOT_H
#define PROGRAM_SELECTION_SNAPSHOT_H

/// Last *other* program slot index for quick toggle with the swap control — not undo of edits within a slot.
struct PreviousProgramIndex {
    bool valid = false;
    int  programIndex = 0;
};

extern PreviousProgramIndex g_previous_program_index;

#endif
