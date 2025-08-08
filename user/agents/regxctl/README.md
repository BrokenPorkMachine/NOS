# regxctl

`regxctl` is a userland helper for inspecting the RegX registry. The skeleton
implementation demonstrates how a CLI could expose the registry via system
calls:

```bash
regxctl list            # enumerate entries
regxctl query <id>      # show basic info
regxctl manifest <id>   # dump manifest details
regxctl tree            # print device hierarchy
```

The current implementation only defines the CLI structure; actual system call
bindings depend on the future NitrOS userland ABI.
