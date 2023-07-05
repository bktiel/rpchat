#!/usr/bin/env bash
for i in {5..1}; do exec python3 client.py -u bean; sleep 0.1; done
