/*
 * Copyright (c) 2016      Intel Corporation. All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */
#include <iostream>
#include "orcmg_tests.h"

extern "C" {
#include "orcm/runtime/orcm_globals.h"
};

using namespace std;

TEST_F(ut_orcm_globals_tests, str_state_undef){
   int rc = strcmp(orcm_node_state_to_str(ORCM_NODE_STATE_UNDEF), "UNDEF");
   ASSERT_EQ(0, rc);
}

TEST_F(ut_orcm_globals_tests, str_state_unknown){
   int rc = strcmp(orcm_node_state_to_str(ORCM_NODE_STATE_UNKNOWN), "UNKNOWN");
   ASSERT_EQ(0, rc);
}

TEST_F(ut_orcm_globals_tests, str_state_up){
   int rc = strcmp(orcm_node_state_to_str(ORCM_NODE_STATE_UP), "UP");
   ASSERT_EQ(0, rc);
}

TEST_F(ut_orcm_globals_tests, str_state_down){
   int rc = strcmp(orcm_node_state_to_str(ORCM_NODE_STATE_DOWN), "DOWN");
   ASSERT_EQ(0, rc);
}

TEST_F(ut_orcm_globals_tests, str_state_sesterm){
   int rc = strcmp(orcm_node_state_to_str(ORCM_NODE_STATE_SESTERM), "SESSION TERMINATING");
   ASSERT_EQ(0, rc);
}

TEST_F(ut_orcm_globals_tests, str_state_drain){
   int rc = strcmp(orcm_node_state_to_str(ORCM_NODE_STATE_DRAIN), "DRAIN");
   ASSERT_EQ(0, rc);
}

TEST_F(ut_orcm_globals_tests, str_state_resume){
   int rc = strcmp(orcm_node_state_to_str(ORCM_NODE_STATE_RESUME), "RESUME");
   ASSERT_EQ(0, rc);
}

TEST_F(ut_orcm_globals_tests, str_state_default){
   int rc = strcmp(orcm_node_state_to_str(-1), "STATEUNDEF");
   ASSERT_EQ(0, rc);
}

TEST_F(ut_orcm_globals_tests, char_state_unknown){
   int rc = strcmp(orcm_node_state_to_char(ORCM_NODE_STATE_UNKNOWN), "?");
   ASSERT_EQ(0, rc);
}

TEST_F(ut_orcm_globals_tests, char_state_up){
   int rc = strcmp(orcm_node_state_to_char(ORCM_NODE_STATE_UP), "U");
   ASSERT_EQ(0, rc);
}

TEST_F(ut_orcm_globals_tests, char_state_down){
   int rc = strcmp(orcm_node_state_to_char(ORCM_NODE_STATE_DOWN), "D");
   ASSERT_EQ(0, rc);
}

TEST_F(ut_orcm_globals_tests, char_state_sesterm){
   int rc = strcmp(orcm_node_state_to_char(ORCM_NODE_STATE_SESTERM), "T");
   ASSERT_EQ(0, rc);
}

TEST_F(ut_orcm_globals_tests, char_state_drain){
   int rc = strcmp(orcm_node_state_to_char(ORCM_NODE_STATE_DRAIN), "X");
   ASSERT_EQ(0, rc);
}

TEST_F(ut_orcm_globals_tests, char_state_resume){
   int rc = strcmp(orcm_node_state_to_char(ORCM_NODE_STATE_RESUME), "R");
   ASSERT_EQ(0, rc);
}

TEST_F(ut_orcm_globals_tests, char_state_default){
   int rc = strcmp(orcm_node_state_to_char(-1), "?");
   ASSERT_EQ(0, rc);
}
