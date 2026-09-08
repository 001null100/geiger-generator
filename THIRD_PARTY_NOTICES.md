# Third-party notices

Geiger Generator uses the following pinned source dependencies:

| Dependency | Revision | Licence / source |
| --- | --- | --- |
| null-clap | 8612a19fbe7fbe81b249c22e1593da18bb998b97 | MIT; https://github.com/001null100/null-clap |
| CLAP | a47f6badb49d948fd009998f28309cdab78979c9 | MIT; https://github.com/free-audio/clap |
| clap-helpers | c35dd4906bd8efbb900cb2b89e680fed463cc8b1 | MIT; https://github.com/free-audio/clap-helpers |
| JUCE | 9.0.0 | Used under AGPLv3; https://github.com/juce-framework/JUCE/tree/9.0.0 |

JUCE's own LICENSE.md identifies embedded dependencies and their licences. The binary package includes copies of that document and licence/notice texts from the used GUI, graphics, events, data structures and core modules. This project does not bundle font files or use JUCE audio-plugin wrapper modules.

The source distribution is the exact repository commit identified in each release together with the pinned dependencies above. CMake fetches those revisions; the dependency-sources workflow can also produce an offline source bundle. Original project code is licensed under AGPL-3.0-only. No third-party sound recordings or commercial meter samples are used.
