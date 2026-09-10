# Third-Party Notices

ArxFatalisHeadTracking bundles, statically links, or credits the third-party components
listed below. Each remains the property of its authors and is used under its own
licence. Where a licence requires the copyright notice, the conditions and the
disclaimer to accompany a binary distribution, the full text is reproduced here
verbatim, and this file ships at the root of every release ZIP we publish.

No Arx Fatalis code, extracted assets or data files are redistributed here. The
one piece of game footage this repository holds is the README demo clip, which
has its own section at the end. What the source does hold about the game is set
out in full under "Arx Fatalis and the DANAE / EERIE engine" below: engine struct offsets, a rotation convention and a focal-to-field-of-view
table read from Arkane Studios' own GPL source release, and the load addresses
of a handful of functions and globals in one shipped build, which let the mod
find them at runtime.

| Component | Version | Licence | How it ships |
|-----------|---------|---------|--------------|
| Ultimate ASI Loader | v9.7.2 | MIT | Bundled in the installer ZIP, modified (embedded third-party DLLs stripped) |
| injector | `f7fd18f` (inside Ultimate ASI Loader v9.7.2) | Zlib | Compiled into the vendored dinput8.dll |
| miniz | 3.0.0 (inside Ultimate ASI Loader v9.7.2) | MIT | Compiled into the vendored dinput8.dll |
| MemoryModule | `5f83e41` (inside Ultimate ASI Loader v9.7.2) | MPL-2.0 | Compiled into the vendored dinput8.dll |
| d3d8to9 | `65870f2` (inside Ultimate ASI Loader v9.7.2) | BSD-2-Clause | Compiled into the vendored dinput8.dll |
| MinHook | v1.3.4 | BSD-2-Clause | Compiled into `ArxFatalisHeadTracking.asi` |
| cameraunlock-core | 152b4716ce9102da6dfff068eaed1ad929ba54b3 | MIT | Compiled into `ArxFatalisHeadTracking.asi` |
| OpenTrack | n/a | ISC | Not bundled; UDP protocol interoperability only |

---

## Ultimate ASI Loader

Vendored at `vendor/ultimate-asi-loader/dinput8.dll`, used as the install-time
source and shipped in the installer ZIP under that name. `install.cmd` copies it
next to `arx.exe` as `dinput.dll`, which is the import slot the game already
has; the loader picks what to forward from whatever name it ends up under at run
time. The upstream licence file ships beside it at
`vendor/ultimate-asi-loader/LICENSE` and again as
`licenses/ultimate-asi-loader-LICENSE.txt`. The binary is modified: see
"Embedded third-party DLLs removed" at the end of this section.

- Upstream: https://github.com/ThirteenAG/Ultimate-ASI-Loader
- Version: `v9.7.2`
- Commit: `ab722befd52581a34449b603926cfab476e66b05`
- Upstream asset SHA-256: `c7277e832f6f07af64903a99ecebab2936260cbf55eda70787c5d7b2d5b9fe60`
- Vendored SHA-256: `867f4cbc96cc2a13d1bd9bf717a6d84c07e0ff72c1a8c171d00b2b8a506ebe90`

```
MIT License

Copyright (c) 2023 ThirteenAG

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
```

That `dinput8.dll` is a static binary and is not one component. The
`Ultimate-ASI-Loader-Win32` target in `premake5.lua` at v9.7.2 compiles
`external/injector/minhook/src/**.c`,
`external/injector/utility/FunctionHookMinHook.cpp`, `external/miniz/miniz.c`,
`external/MemoryModule/*.c` and `external/d3d8to9/source/*.cpp` alongside the
loader's own sources, and builds against the DirectX 9 SDK headers and import
libraries in `external/minidx9`. Redistributing it therefore redistributes
MinHook, injector, miniz, MemoryModule and d3d8to9 as well, and each has its own
section in this file. The MinHook section covers the copy inside the loader as
well as any linked into the mod itself; the licence text is the same.

### Embedded third-party DLLs removed

Compiled-in components are not the whole of it. The upstream 32-bit
`dinput8.dll` also carries three complete third-party DLLs as `RCDATA`
resources, so that a user who renames the loader over one of those libraries
still gets the original exports, plus the ini template one of them reads. Each
was identified from the export table and version resource inside the resource
itself, in the vendored file:

| Resource | Identifies itself as | Rights holder |
|----------|----------------------|---------------|
| `RCDATA 101` | `vorbisfile.dll` | Xiph.Org Foundation, BSD-3-Clause |
| `RCDATA 103` | `wndmode.dll`, "DirectX Windower Embedded v2.3 (based on D3D Windower v1.88)" | (C) 2008 VEG, (C) 2004 menopem |
| `RCDATA 104` | `wndmode.ini` template for the above | as above |
| `RCDATA 105` | `binkw32.dll`, "Bink and Smacker" 1.994i | Copyright (C) 1994-2014, RAD Game Tools, Inc. |

None of the three is ours to hand on. Bink is proprietary middleware licensed
per title, and a release ZIP is a redistribution however deeply the bytes are
buried. The windower carries no licence at all. vorbisfile is redistributable
under BSD-3-Clause, but only with its notice, and we neither call it nor want
the obligation.

So the vendored copy has all four resources zeroed by
`scripts/strip-loader-payload.ps1`, which `pixi run update-deps` runs on every
refresh and which `scripts/package-release.ps1` re-runs in `-VerifyOnly` mode
before the ZIP is built, so a loader that still carries them cannot reach a
release. Only the `.rsrc` section differs from the upstream asset: the loader's
code, data, imports, relocations and appended PDB are byte-identical, which is
what the two hashes above are for. MIT permits the modification, and it is
recorded here and in `vendor/ultimate-asi-loader/README.md` so this copy is not
mistaken for stock upstream.

Nothing in this mod could reach those resources in any case. The two library
payloads are selected by the loader's own filename, and we deploy it as
`dinput.dll`; the windower needs a `wndmode.ini` next to the game exe, which we
never ship.

---

## injector

Compiled into the vendored `dinput8.dll`. The loader's `FunctionHookMinHook`
wrapper, which the `Ultimate-ASI-Loader-Win32` target compiles from
`external/injector/utility/FunctionHookMinHook.cpp`, and the MinHook submodule
that repository carries. Nothing in this repository calls or links it; it ships
only inside that binary.

- Upstream: https://github.com/ThirteenAG/injector
- Version: commit `f7fd18f7fcb4691f470b7a047697e591a39a94fc`, the submodule
  Ultimate ASI Loader v9.7.2 pins at `external/injector/`
- Licence: Zlib

Its code inside that binary is unaltered upstream - the only change we make to
the loader is zeroing the embedded resources described above, which touches no
compiled code - so the "altered source versions" condition below does not
arise. It is reproduced whole regardless.

```
Copyright (C) 2012-2014 LINK/2012 <dma_2012@hotmail.com>

This software is provided 'as-is', without any express or implied
warranty. In no event will the authors be held liable for any damages
arising from the use of this software.

Permission is granted to anyone to use this software for any purpose,
including commercial applications, and to alter it and redistribute it
freely, subject to the following restrictions:

   1. The origin of this software must not be misrepresented; you must not
   claim that you wrote the original software. If you use this software
   in a product, an acknowledgment in the product documentation would be
   appreciated but is not required.

   2. Altered source versions must be plainly marked as such, and must not be
   misrepresented as being the original software.

   3. This notice may not be removed or altered from any source
   distribution.
```

---

## miniz

Compiled into the vendored `dinput8.dll`. Zip reading for the loader's
`LoadVirtualFilesFromZip` path, which the `Ultimate-ASI-Loader-Win32` target
compiles from `external/miniz/miniz.c`. Nothing in this repository calls or
links it; it ships only inside that binary.

- Upstream: https://github.com/richgel999/miniz
- Version: 3.0.0, as vendored at `external/miniz/` in Ultimate ASI Loader v9.7.2
- Licence: MIT

```
Copyright 2013-2014 RAD Game Tools and Valve Software
Copyright 2010-2014 Rich Geldreich and Tenacious Software LLC

All Rights Reserved.

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
```

---

## MemoryModule

Compiled into the vendored `dinput8.dll`. Loads a DLL image from memory rather
than from a file. The `Ultimate-ASI-Loader-Win32` target compiles it from
`external/MemoryModule/*.c`; it is not part of the x64 build. Nothing in this
repository calls or links it; it ships only inside that binary.

- Upstream: https://github.com/fancycode/MemoryModule
- Version: commit `5f83e41c3a3e7c6e8284a5c1afa5a38790809461`, the submodule
  Ultimate ASI Loader v9.7.2 pins at `external/MemoryModule/`
- Licence: MPL-2.0

**Source Code Form.** MemoryModule is licensed under the Mozilla Public License
2.0, and section 3.2 of that licence requires anyone who distributes it in
Executable Form to tell recipients how to obtain its Source Code Form. The copy
inside this `dinput8.dll` is the unmodified upstream at commit
`5f83e41c3a3e7c6e8284a5c1afa5a38790809461`, the submodule Ultimate ASI Loader
v9.7.2 pins at `external/MemoryModule/`. Its complete source is available under
that same licence from a copy itsloopyo keeps at
https://github.com/itsloopyo/MemoryModule/tree/5f83e41c3a3e7c6e8284a5c1afa5a38790809461,
from the upstream repository at
https://github.com/fancycode/MemoryModule/tree/5f83e41c3a3e7c6e8284a5c1afa5a38790809461,
and inside Ultimate ASI Loader's own source tree at
https://github.com/ThirteenAG/Ultimate-ASI-Loader/tree/v9.7.2.

```
Mozilla Public License Version 2.0
==================================

1. Definitions
--------------

1.1. "Contributor"
    means each individual or legal entity that creates, contributes to
    the creation of, or owns Covered Software.

1.2. "Contributor Version"
    means the combination of the Contributions of others (if any) used
    by a Contributor and that particular Contributor's Contribution.

1.3. "Contribution"
    means Covered Software of a particular Contributor.

1.4. "Covered Software"
    means Source Code Form to which the initial Contributor has attached
    the notice in Exhibit A, the Executable Form of such Source Code
    Form, and Modifications of such Source Code Form, in each case
    including portions thereof.

1.5. "Incompatible With Secondary Licenses"
    means

    (a) that the initial Contributor has attached the notice described
        in Exhibit B to the Covered Software; or

    (b) that the Covered Software was made available under the terms of
        version 1.1 or earlier of the License, but not also under the
        terms of a Secondary License.

1.6. "Executable Form"
    means any form of the work other than Source Code Form.

1.7. "Larger Work"
    means a work that combines Covered Software with other material, in 
    a separate file or files, that is not Covered Software.

1.8. "License"
    means this document.

1.9. "Licensable"
    means having the right to grant, to the maximum extent possible,
    whether at the time of the initial grant or subsequently, any and
    all of the rights conveyed by this License.

1.10. "Modifications"
    means any of the following:

    (a) any file in Source Code Form that results from an addition to,
        deletion from, or modification of the contents of Covered
        Software; or

    (b) any new file in Source Code Form that contains any Covered
        Software.

1.11. "Patent Claims" of a Contributor
    means any patent claim(s), including without limitation, method,
    process, and apparatus claims, in any patent Licensable by such
    Contributor that would be infringed, but for the grant of the
    License, by the making, using, selling, offering for sale, having
    made, import, or transfer of either its Contributions or its
    Contributor Version.

1.12. "Secondary License"
    means either the GNU General Public License, Version 2.0, the GNU
    Lesser General Public License, Version 2.1, the GNU Affero General
    Public License, Version 3.0, or any later versions of those
    licenses.

1.13. "Source Code Form"
    means the form of the work preferred for making modifications.

1.14. "You" (or "Your")
    means an individual or a legal entity exercising rights under this
    License. For legal entities, "You" includes any entity that
    controls, is controlled by, or is under common control with You. For
    purposes of this definition, "control" means (a) the power, direct
    or indirect, to cause the direction or management of such entity,
    whether by contract or otherwise, or (b) ownership of more than
    fifty percent (50%) of the outstanding shares or beneficial
    ownership of such entity.

2. License Grants and Conditions
--------------------------------

2.1. Grants

Each Contributor hereby grants You a world-wide, royalty-free,
non-exclusive license:

(a) under intellectual property rights (other than patent or trademark)
    Licensable by such Contributor to use, reproduce, make available,
    modify, display, perform, distribute, and otherwise exploit its
    Contributions, either on an unmodified basis, with Modifications, or
    as part of a Larger Work; and

(b) under Patent Claims of such Contributor to make, use, sell, offer
    for sale, have made, import, and otherwise transfer either its
    Contributions or its Contributor Version.

2.2. Effective Date

The licenses granted in Section 2.1 with respect to any Contribution
become effective for each Contribution on the date the Contributor first
distributes such Contribution.

2.3. Limitations on Grant Scope

The licenses granted in this Section 2 are the only rights granted under
this License. No additional rights or licenses will be implied from the
distribution or licensing of Covered Software under this License.
Notwithstanding Section 2.1(b) above, no patent license is granted by a
Contributor:

(a) for any code that a Contributor has removed from Covered Software;
    or

(b) for infringements caused by: (i) Your and any other third party's
    modifications of Covered Software, or (ii) the combination of its
    Contributions with other software (except as part of its Contributor
    Version); or

(c) under Patent Claims infringed by Covered Software in the absence of
    its Contributions.

This License does not grant any rights in the trademarks, service marks,
or logos of any Contributor (except as may be necessary to comply with
the notice requirements in Section 3.4).

2.4. Subsequent Licenses

No Contributor makes additional grants as a result of Your choice to
distribute the Covered Software under a subsequent version of this
License (see Section 10.2) or under the terms of a Secondary License (if
permitted under the terms of Section 3.3).

2.5. Representation

Each Contributor represents that the Contributor believes its
Contributions are its original creation(s) or it has sufficient rights
to grant the rights to its Contributions conveyed by this License.

2.6. Fair Use

This License is not intended to limit any rights You have under
applicable copyright doctrines of fair use, fair dealing, or other
equivalents.

2.7. Conditions

Sections 3.1, 3.2, 3.3, and 3.4 are conditions of the licenses granted
in Section 2.1.

3. Responsibilities
-------------------

3.1. Distribution of Source Form

All distribution of Covered Software in Source Code Form, including any
Modifications that You create or to which You contribute, must be under
the terms of this License. You must inform recipients that the Source
Code Form of the Covered Software is governed by the terms of this
License, and how they can obtain a copy of this License. You may not
attempt to alter or restrict the recipients' rights in the Source Code
Form.

3.2. Distribution of Executable Form

If You distribute Covered Software in Executable Form then:

(a) such Covered Software must also be made available in Source Code
    Form, as described in Section 3.1, and You must inform recipients of
    the Executable Form how they can obtain a copy of such Source Code
    Form by reasonable means in a timely manner, at a charge no more
    than the cost of distribution to the recipient; and

(b) You may distribute such Executable Form under the terms of this
    License, or sublicense it under different terms, provided that the
    license for the Executable Form does not attempt to limit or alter
    the recipients' rights in the Source Code Form under this License.

3.3. Distribution of a Larger Work

You may create and distribute a Larger Work under terms of Your choice,
provided that You also comply with the requirements of this License for
the Covered Software. If the Larger Work is a combination of Covered
Software with a work governed by one or more Secondary Licenses, and the
Covered Software is not Incompatible With Secondary Licenses, this
License permits You to additionally distribute such Covered Software
under the terms of such Secondary License(s), so that the recipient of
the Larger Work may, at their option, further distribute the Covered
Software under the terms of either this License or such Secondary
License(s).

3.4. Notices

You may not remove or alter the substance of any license notices
(including copyright notices, patent notices, disclaimers of warranty,
or limitations of liability) contained within the Source Code Form of
the Covered Software, except that You may alter any license notices to
the extent required to remedy known factual inaccuracies.

3.5. Application of Additional Terms

You may choose to offer, and to charge a fee for, warranty, support,
indemnity or liability obligations to one or more recipients of Covered
Software. However, You may do so only on Your own behalf, and not on
behalf of any Contributor. You must make it absolutely clear that any
such warranty, support, indemnity, or liability obligation is offered by
You alone, and You hereby agree to indemnify every Contributor for any
liability incurred by such Contributor as a result of warranty, support,
indemnity or liability terms You offer. You may include additional
disclaimers of warranty and limitations of liability specific to any
jurisdiction.

4. Inability to Comply Due to Statute or Regulation
---------------------------------------------------

If it is impossible for You to comply with any of the terms of this
License with respect to some or all of the Covered Software due to
statute, judicial order, or regulation then You must: (a) comply with
the terms of this License to the maximum extent possible; and (b)
describe the limitations and the code they affect. Such description must
be placed in a text file included with all distributions of the Covered
Software under this License. Except to the extent prohibited by statute
or regulation, such description must be sufficiently detailed for a
recipient of ordinary skill to be able to understand it.

5. Termination
--------------

5.1. The rights granted under this License will terminate automatically
if You fail to comply with any of its terms. However, if You become
compliant, then the rights granted under this License from a particular
Contributor are reinstated (a) provisionally, unless and until such
Contributor explicitly and finally terminates Your grants, and (b) on an
ongoing basis, if such Contributor fails to notify You of the
non-compliance by some reasonable means prior to 60 days after You have
come back into compliance. Moreover, Your grants from a particular
Contributor are reinstated on an ongoing basis if such Contributor
notifies You of the non-compliance by some reasonable means, this is the
first time You have received notice of non-compliance with this License
from such Contributor, and You become compliant prior to 30 days after
Your receipt of the notice.

5.2. If You initiate litigation against any entity by asserting a patent
infringement claim (excluding declaratory judgment actions,
counter-claims, and cross-claims) alleging that a Contributor Version
directly or indirectly infringes any patent, then the rights granted to
You by any and all Contributors for the Covered Software under Section
2.1 of this License shall terminate.

5.3. In the event of termination under Sections 5.1 or 5.2 above, all
end user license agreements (excluding distributors and resellers) which
have been validly granted by You or Your distributors under this License
prior to termination shall survive termination.

************************************************************************
*                                                                      *
*  6. Disclaimer of Warranty                                           *
*  -------------------------                                           *
*                                                                      *
*  Covered Software is provided under this License on an "as is"       *
*  basis, without warranty of any kind, either expressed, implied, or  *
*  statutory, including, without limitation, warranties that the       *
*  Covered Software is free of defects, merchantable, fit for a        *
*  particular purpose or non-infringing. The entire risk as to the     *
*  quality and performance of the Covered Software is with You.        *
*  Should any Covered Software prove defective in any respect, You     *
*  (not any Contributor) assume the cost of any necessary servicing,   *
*  repair, or correction. This disclaimer of warranty constitutes an   *
*  essential part of this License. No use of any Covered Software is   *
*  authorized under this License except under this disclaimer.         *
*                                                                      *
************************************************************************

************************************************************************
*                                                                      *
*  7. Limitation of Liability                                          *
*  --------------------------                                          *
*                                                                      *
*  Under no circumstances and under no legal theory, whether tort      *
*  (including negligence), contract, or otherwise, shall any           *
*  Contributor, or anyone who distributes Covered Software as          *
*  permitted above, be liable to You for any direct, indirect,         *
*  special, incidental, or consequential damages of any character      *
*  including, without limitation, damages for lost profits, loss of    *
*  goodwill, work stoppage, computer failure or malfunction, or any    *
*  and all other commercial damages or losses, even if such party      *
*  shall have been informed of the possibility of such damages. This   *
*  limitation of liability shall not apply to liability for death or   *
*  personal injury resulting from such party's negligence to the       *
*  extent applicable law prohibits such limitation. Some               *
*  jurisdictions do not allow the exclusion or limitation of           *
*  incidental or consequential damages, so this exclusion and          *
*  limitation may not apply to You.                                    *
*                                                                      *
************************************************************************

8. Litigation
-------------

Any litigation relating to this License may be brought only in the
courts of a jurisdiction where the defendant maintains its principal
place of business and such litigation shall be governed by laws of that
jurisdiction, without reference to its conflict-of-law provisions.
Nothing in this Section shall prevent a party's ability to bring
cross-claims or counter-claims.

9. Miscellaneous
----------------

This License represents the complete agreement concerning the subject
matter hereof. If any provision of this License is held to be
unenforceable, such provision shall be reformed only to the extent
necessary to make it enforceable. Any law or regulation which provides
that the language of a contract shall be construed against the drafter
shall not be used to construe this License against a Contributor.

10. Versions of the License
---------------------------

10.1. New Versions

Mozilla Foundation is the license steward. Except as provided in Section
10.3, no one other than the license steward has the right to modify or
publish new versions of this License. Each version will be given a
distinguishing version number.

10.2. Effect of New Versions

You may distribute the Covered Software under the terms of the version
of the License under which You originally received the Covered Software,
or under the terms of any subsequent version published by the license
steward.

10.3. Modified Versions

If you create software not governed by this License, and you want to
create a new license for such software, you may create and use a
modified version of this License if you rename the license and remove
any references to the name of the license steward (except to note that
such modified license differs from this License).

10.4. Distributing Source Code Form that is Incompatible With Secondary
Licenses

If You choose to distribute Source Code Form that is Incompatible With
Secondary Licenses under the terms of this version of the License, the
notice described in Exhibit B of this License must be attached.

Exhibit A - Source Code Form License Notice
-------------------------------------------

  This Source Code Form is subject to the terms of the Mozilla Public
  License, v. 2.0. If a copy of the MPL was not distributed with this
  file, You can obtain one at http://mozilla.org/MPL/2.0/.

If it is not possible or desirable to put the notice in a particular
file, then You may include the notice in a location (such as a LICENSE
file in a relevant directory) where a recipient would be likely to look
for such a notice.

You may add additional accurate notices of copyright ownership.

Exhibit B - "Incompatible With Secondary Licenses" Notice
---------------------------------------------------------

  This Source Code Form is "Incompatible With Secondary Licenses", as
  defined by the Mozilla Public License, v. 2.0.
```

---

## d3d8to9

Compiled into the vendored `dinput8.dll`. The Direct3D 8 to Direct3D 9
conversion layer behind the loader's `used3d8to9` ini switch, which the
`Ultimate-ASI-Loader-Win32` target compiles from
`external/d3d8to9/source/*.cpp`; it is not part of the x64 build. Nothing in
this repository calls or links it; it ships only inside that binary.

- Upstream: https://github.com/crosire/d3d8to9
- Version: commit `65870f2302e9c496cd6d873d6095961d5c777668`, the submodule
  Ultimate ASI Loader v9.7.2 pins at `external/d3d8to9/`
- Licence: BSD-2-Clause

```
Copyright (C) 2015 Patrick Mours. All rights reserved.

Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:

  * Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.
  * Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer in the documentation and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
```

---

## MinHook

Source committed at `cameraunlock-core/vendor/minhook/` and compiled into `ArxFatalisHeadTracking.asi`. The
committed tree is the authoritative record of exactly what is built.

- Upstream: https://github.com/TsudaKageyu/minhook
- Version: `v1.3.4`, modified (see below). Upstream's own CMakeLists at that tag
  still declares `MINHOOK_PATCH_VERSION 3`, so the compiled-in `MINHOOK_VERSION`
  constant reads 1.3.3 on a v1.3.4 tree; the vendored source is the v1.3.4
  release. The committed tree at
  `cameraunlock-core/vendor/minhook/` records the baseline and every local change
- Licence: BSD-2-Clause
- Bundled: yes, compiled into `ArxFatalisHeadTracking.asi` in the release ZIP

MinHook carries two copyright holders: Tsuda Kageyu for MinHook itself, and
Vyacheslav Patkov for the Hacker Disassembler Engine that `src/hde/` is built
from. Both notices appear below exactly as upstream ships them.

This copy is modified: `MH_Initialize` uses `GetProcessHeap()` rather than
standing up a private heap with `HeapCreate`, and `MH_Uninitialize` skips the
matching `HeapDestroy`. BSD-2-Clause permits the change; it is recorded here so
the attribution is not mistaken for a claim of an unmodified copy.

```
MinHook - The Minimalistic API Hooking Library for x64/x86
Copyright (C) 2009-2017 Tsuda Kageyu.
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions
are met:

 1. Redistributions of source code must retain the above copyright
    notice, this list of conditions and the following disclaimer.
 2. Redistributions in binary form must reproduce the above copyright
    notice, this list of conditions and the following disclaimer in the
    documentation and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
"AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER
OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

================================================================================
Portions of this software are Copyright (c) 2008-2009, Vyacheslav Patkov.
================================================================================
Hacker Disassembler Engine 32 C
Copyright (c) 2008-2009, Vyacheslav Patkov.
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions
are met:

 1. Redistributions of source code must retain the above copyright
    notice, this list of conditions and the following disclaimer.
 2. Redistributions in binary form must reproduce the above copyright
    notice, this list of conditions and the following disclaimer in the
    documentation and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
"AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE REGENTS OR
CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

-------------------------------------------------------------------------------
Hacker Disassembler Engine 64 C
Copyright (c) 2008-2009, Vyacheslav Patkov.
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions
are met:

 1. Redistributions of source code must retain the above copyright
    notice, this list of conditions and the following disclaimer.
 2. Redistributions in binary form must reproduce the above copyright
    notice, this list of conditions and the following disclaimer in the
    documentation and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
"AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE REGENTS OR
CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
```

---

## cameraunlock-core

Git submodule at `cameraunlock-core/`, compiled into `ArxFatalisHeadTracking.asi`. Our own code,
MIT licensed, reproduced here so the notices are complete.

- Upstream: https://github.com/itsloopyo/cameraunlock-core
- Pinned commit: `152b4716ce9102da6dfff068eaed1ad929ba54b3`
- Licence: MIT
- Bundled: yes, compiled into `ArxFatalisHeadTracking.asi` in the release ZIP

```
MIT License

Copyright (c) 2026 itsloopyo

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
```

---

## OpenTrack

Not bundled and not linked. This mod implements the OpenTrack UDP pose datagram
layout so that OpenTrack (https://github.com/opentrack/opentrack, ISC licence)
and compatible trackers can drive it. No OpenTrack code, headers or binaries
are copied, linked or redistributed, so its licence triggers no notice
obligation here. It is credited because the wire format is its work.

---
## Arx Fatalis and the DANAE / EERIE engine

- Copyright (c) Arkane Studios. All rights reserved.
- Not licensed to this project, not redistributed by it, and not affiliated
  with it.

This mod is an unofficial, independent modification. It requires a
legitimately purchased copy of the game, which it never bypasses, patches on
disk, or redistributes. **No game code, no extracted game assets, no data
files, no executables and no game source of any kind is contained in this
repository or in any release ZIP**, the README demo clip covered under "Arx
Fatalis footage" below excepted. The mod hooks the running process in memory at
runtime and ships nothing but its own compiled `.asi`.

Arkane Studios released the Arx Fatalis game and engine code as open source on
14 January 2011 under the **GNU General Public License version 3**, with
additional terms recorded in that release (`ARX_PUBLIC_LICENSE.txt`), which
replace the GPL's sections 15 and 16, grant no trademark licence, and require
that the origin of the program not be misrepresented. The `ArxFatalis-1.21`
tag in the Arx Libertatis repository is that drop unmodified:

- https://github.com/arx/ArxLibertatis

That source release is where the `EERIE_CAMERA` and `EERIE_TRANSFORM` member
names and offsets, the `ARXCHARACTER`, `EERIEPOLY` and `EERIE_VERTEX` layouts,
the `D3DUtil_SetViewMatrix` signature, the yaw-then-pitch-then-roll
rotation order, the sign of each of `angle.a` / `angle.b` / `angle.g`, the
focal-to-field-of-view mapping and the `EERIELaunchRay3` and
`EERIEDrawBitmap` signatures recorded in `src/arx_game.h`,
`src/build_profile.cpp` and `src/camera_hook.cpp` come from. We are grateful
for it, and record the debt here rather than leaving it unstated.

**No GPL-licensed Arx Fatalis code is copied, adapted, translated, linked, or
distributed by this project.** What is used from that source is factual
information about a binary interface - member offsets, a rotation order, a
piecewise constant table, a function signature - in order to interoperate with
the shipped game at runtime. Every offset was confirmed against the running
executable before it was used, and the projection in `src/arx_game.h` is an
independent implementation written to match the engine's convention.

**Contributors: do not paste code from the Arx Fatalis source release into
this repository.** It is GPL and this project is MIT; copying any of it here
would be a licence violation and would relicense work that is not ours to
relicense. Reference it, describe it, re-derive it - never copy it.

The addresses in `src/build_profile.cpp` are the load addresses of functions
and globals in one specific shipped build of `arx.exe`, matched by that build's
PE header. They exist solely so the mod can find those symbols at runtime, they
are the minimum needed for interoperability, and none is usable as game code.

---

## Hacker Disassembler Engine 32/64 (HDE)

- Copyright (c) 2008-2009 Vyacheslav Patkov. All rights reserved.
- License: BSD-2-Clause (reproduced in `cameraunlock-core/vendor/minhook/LICENSE.txt`)
- Included as part of MinHook under `cameraunlock-core/vendor/minhook/src/hde/` and compiled into
  the mod `.asi`. It is a separate work by a separate copyright holder from
  MinHook and is named here so the binary distribution carries its attribution.

---

## Arx Fatalis

Arx Fatalis and all related names, logos, characters and marks are trademarks
of their respective owners. They are used here only to identify the game this
mod applies to, which is nominative use and not a claim of any right in them.
This project is an unofficial, fan-made modification. It is not affiliated
with, endorsed by, or sponsored by the game's developers, its publishers, its
engine vendor, or any other rights holder. It redistributes no game code, no
extracted game assets and no proprietary DLLs, and it requires a legitimately
purchased copy of the game. Where the source records engine structure offsets, a
rotation convention or a load address, the section above says exactly where
each came from and what it is; no game source of any kind is stored in this
repository.

---

## Arx Fatalis footage

- File: `assets/readme-clip.gif`
- Rights holder: Arkane Studios (game), ZeniMax Media (publisher)
- Purpose: a short gameplay clip at the top of the README showing what the mod
  does, the same nominative use every mod page makes of the game it is for
- Kept in this repository; it ships in no release ZIP
- No licence is granted over it by this project. It will be removed on request
  by the rights holder.
