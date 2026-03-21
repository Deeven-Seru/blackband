#!/bin/bash
lldb --batch -o "run" -o "bt" ./agentc -- examples/test_agent.agc -o test_agent
