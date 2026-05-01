#ifndef KILN_SUPERVISOR_H
#define KILN_SUPERVISOR_H

/// Slow-loop observer that evaluates kiln safety/process faults and latches ERROR state.
/// Call from main loop (non-interrupt context).
class KilnSupervisor {
public:
    void service();

private:
    void resetRunWindowState();
};

#endif // KILN_SUPERVISOR_H
