/*
 * Digital Voting System in C
 * --------------------------
 * Features:
 *  - Admin can add candidates, view results, and close the election
 *  - Voters cast one vote each (duplicate voter IDs are rejected)
 *  - Live vote counts, percentages, winner / tie detection
 *  - Votes are saved to disk (votes.dat) so results survive a restart
 *
 * Compile:  gcc -Wall -Wextra -o voting_system voting_system.c
 * Run:      ./voting_system
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_CANDIDATES 10
#define MAX_VOTERS     1000
#define NAME_LEN       50
#define ID_LEN         20
#define ADMIN_PASSWORD "admin123"      /* change before real use */
#define DATA_FILE      "votes.dat"

typedef struct {
    char name[NAME_LEN];
    int  votes;
} Candidate;

typedef struct {
    Candidate candidates[MAX_CANDIDATES];
    int  candidateCount;
    char voterIds[MAX_VOTERS][ID_LEN];  /* IDs of people who have voted */
    int  voterCount;
    int  electionClosed;
} Election;

/* ---------- helpers ---------- */

static void readLine(const char *prompt, char *buf, size_t size) {
    printf("%s", prompt);
    if (fgets(buf, (int)size, stdin) == NULL) {
        buf[0] = '\0';
        return;
    }
    buf[strcspn(buf, "\n")] = '\0';
}

static int readInt(const char *prompt) {
    char line[32];
    char *end;
    long v;
    for (;;) {
        readLine(prompt, line, sizeof line);
        v = strtol(line, &end, 10);
        if (end != line && *end == '\0') return (int)v;
        printf("Invalid input. Please enter a number.\n");
    }
}

static int totalVotes(const Election *e) {
    int total = 0;
    for (int i = 0; i < e->candidateCount; i++) total += e->candidates[i].votes;
    return total;
}

static int hasVoted(const Election *e, const char *id) {
    for (int i = 0; i < e->voterCount; i++)
        if (strcmp(e->voterIds[i], id) == 0) return 1;
    return 0;
}

/* ---------- persistence ---------- */

static void saveElection(const Election *e) {
    FILE *f = fopen(DATA_FILE, "wb");
    if (!f) { printf("Warning: could not save data.\n"); return; }
    fwrite(e, sizeof *e, 1, f);
    fclose(f);
}

static void loadElection(Election *e) {
    FILE *f = fopen(DATA_FILE, "rb");
    memset(e, 0, sizeof *e);
    if (!f) return;
    if (fread(e, sizeof *e, 1, f) != 1) memset(e, 0, sizeof *e);
    fclose(f);
}

/* ---------- admin functions ---------- */

static int adminLogin(void) {
    char pw[32];
    readLine("Enter admin password: ", pw, sizeof pw);
    if (strcmp(pw, ADMIN_PASSWORD) == 0) return 1;
    printf("Incorrect password.\n");
    return 0;
}

static void addCandidate(Election *e) {
    char name[NAME_LEN];
    if (e->electionClosed) { printf("Election is closed.\n"); return; }
    if (e->voterCount > 0) {
        printf("Cannot add candidates after voting has started.\n");
        return;
    }
    if (e->candidateCount >= MAX_CANDIDATES) {
        printf("Maximum number of candidates reached.\n");
        return;
    }
    readLine("Candidate name: ", name, sizeof name);
    if (name[0] == '\0') { printf("Name cannot be empty.\n"); return; }
    for (int i = 0; i < e->candidateCount; i++) {
        if (strcmp(e->candidates[i].name, name) == 0) {
            printf("Candidate already exists.\n");
            return;
        }
    }
    strcpy(e->candidates[e->candidateCount].name, name);
    e->candidates[e->candidateCount].votes = 0;
    e->candidateCount++;
    saveElection(e);
    printf("Candidate \"%s\" added.\n", name);
}

static void displayResults(const Election *e) {
    int total = totalVotes(e);
    int max = -1, winners = 0;

    if (e->candidateCount == 0) { printf("No candidates registered.\n"); return; }

    printf("\n========== ELECTION RESULTS ==========\n");
    printf("%-4s %-25s %-8s %-8s\n", "No.", "Candidate", "Votes", "Share");
    printf("--------------------------------------------\n");
    for (int i = 0; i < e->candidateCount; i++) {
        double pct = total ? (e->candidates[i].votes * 100.0 / total) : 0.0;
        printf("%-4d %-25s %-8d %6.2f%%\n", i + 1, e->candidates[i].name,
               e->candidates[i].votes, pct);
        if (e->candidates[i].votes > max) max = e->candidates[i].votes;
    }
    printf("--------------------------------------------\n");
    printf("Total votes cast: %d\n", total);
    printf("Status: %s\n", e->electionClosed ? "CLOSED (final)" : "OPEN (provisional)");

    if (total == 0) { printf("No votes have been cast yet.\n"); return; }

    for (int i = 0; i < e->candidateCount; i++)
        if (e->candidates[i].votes == max) winners++;

    if (winners == 1) {
        for (int i = 0; i < e->candidateCount; i++)
            if (e->candidates[i].votes == max)
                printf("%s: %s with %d vote(s)\n",
                       e->electionClosed ? "WINNER" : "Currently leading",
                       e->candidates[i].name, max);
    } else {
        printf("%s between:\n", e->electionClosed ? "TIE" : "Currently tied");
        for (int i = 0; i < e->candidateCount; i++)
            if (e->candidates[i].votes == max)
                printf("  - %s (%d votes)\n", e->candidates[i].name, max);
    }
    printf("======================================\n");
}

static void closeElection(Election *e) {
    char c[8];
    if (e->electionClosed) { printf("Election already closed.\n"); return; }
    readLine("Close the election? No more votes will be accepted (y/n): ", c, sizeof c);
    if (c[0] == 'y' || c[0] == 'Y') {
        e->electionClosed = 1;
        saveElection(e);
        printf("Election closed.\n");
        displayResults(e);
    }
}

static void resetElection(Election *e) {
    char c[8];
    readLine("This erases ALL candidates and votes. Type 'yes' to confirm: ", c, sizeof c);
    if (strcmp(c, "yes") == 0) {
        memset(e, 0, sizeof *e);
        saveElection(e);
        printf("Election reset.\n");
    }
}

static void adminMenu(Election *e) {
    if (!adminLogin()) return;
    for (;;) {
        printf("\n--- ADMIN MENU ---\n"
               "1. Add candidate\n"
               "2. View results\n"
               "3. Close election\n"
               "4. Reset election\n"
               "5. Back\n");
        switch (readInt("Choice: ")) {
            case 1: addCandidate(e);   break;
            case 2: displayResults(e); break;
            case 3: closeElection(e);  break;
            case 4: resetElection(e);  break;
            case 5: return;
            default: printf("Invalid choice.\n");
        }
    }
}

/* ---------- voter function ---------- */

static void castVote(Election *e) {
    char id[ID_LEN];
    char confirm[8];

    if (e->electionClosed)        { printf("Voting is closed.\n"); return; }
    if (e->candidateCount == 0)   { printf("No candidates registered yet.\n"); return; }
    if (e->voterCount >= MAX_VOTERS) { printf("Voter capacity reached.\n"); return; }

    readLine("Enter your Voter ID: ", id, sizeof id);
    if (id[0] == '\0') { printf("Voter ID cannot be empty.\n"); return; }
    if (hasVoted(e, id)) {
        printf("This Voter ID has already voted. Duplicate votes are not allowed.\n");
        return;
    }

    printf("\nCandidates:\n");
    for (int i = 0; i < e->candidateCount; i++)
        printf("  %d. %s\n", i + 1, e->candidates[i].name);

    int choice = readInt("Enter candidate number: ");
    if (choice < 1 || choice > e->candidateCount) {
        printf("Invalid candidate. Vote not recorded.\n");
        return;
    }

    printf("You selected: %s\n", e->candidates[choice - 1].name);
    readLine("Confirm vote? (y/n): ", confirm, sizeof confirm);
    if (confirm[0] != 'y' && confirm[0] != 'Y') {
        printf("Vote cancelled.\n");
        return;
    }

    e->candidates[choice - 1].votes++;
    strcpy(e->voterIds[e->voterCount++], id);
    saveElection(e);
    printf("Thank you! Your vote has been recorded.\n");
}

/* ---------- main ---------- */

int main(void) {
    Election election;
    loadElection(&election);

    printf("=====================================\n");
    printf("      DIGITAL VOTING SYSTEM\n");
    printf("=====================================\n");

    for (;;) {
        printf("\n--- MAIN MENU ---\n"
               "1. Cast vote\n"
               "2. View results\n"
               "3. Admin panel\n"
               "4. Exit\n");
        switch (readInt("Choice: ")) {
            case 1: castVote(&election);       break;
            case 2: displayResults(&election); break;
            case 3: adminMenu(&election);      break;
            case 4: printf("Goodbye!\n"); return 0;
            default: printf("Invalid choice.\n");
        }
    }
}