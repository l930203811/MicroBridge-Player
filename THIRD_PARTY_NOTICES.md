# Third-party notices / 第三方声明

This file lists third-party components bundled with, linked into, or required to
build **MicroBridge Player**. The project's own source code is licensed under the
[GNU AGPLv3](LICENSE); the components below are **not** re-licensed by that grant —
each keeps its own terms.

本文件列出 **MicroBridge Player** 捆绑、链接或构建所需的第三方组件。项目自身源码采用
[GNU AGPLv3](LICENSE)；下列组件**不在**该授权范围内，各自保留其原始许可。

---

## 1. JUCE 8.0.15 — AGPLv3 or commercial

- Source: https://github.com/juce-framework/JUCE (release 8.0.15)
- Included as `ThirdParty/juce-8.0.15/`, compiled into the app and VST3 plugin.
- JUCE is dual-licensed under **AGPLv3** and the commercial JUCE 8 licence.
  This project uses the **AGPLv3** path — see `ThirdParty/juce-8.0.15/LICENSE.md`.
- JUCE bundles further sub-components under their own licences (VST3 SDK MIT,
  Ogg Vorbis / FLAC BSD, AudioUnitSDK Apache-2.0, jpeglib IJG, CHOC ISC, etc.).
  The full list is in `ThirdParty/juce-8.0.15/LICENSE.md`.

## 2. Steinberg ASIO SDK — proprietary

- JUCE's `juce_audio_devices/native/asio/` references the Steinberg ASIO SDK.
- The ASIO SDK is **not** redistributable; it is proprietary and requires a
  licence from Steinberg. No ASIO SDK binary is shipped in this repository or
  in the release archives. If you need ASIO support, obtain the SDK from
  Steinberg and comply with its licence.

## 3. kissfft — BSD-3-Clause

- Source: https://github.com/mborgerding/kissFFT
- Copyright (c) 2003-2010 Mark Borgerding.
- SPDX-License-Identifier: BSD-3-Clause. Full text: `ThirdParty/kissfft/COPYING`.

## 4. ONNX Runtime 1.17.1 — MIT

- Source: https://github.com/microsoft/onnxruntime
- Copyright (c) Microsoft Corporation. Licensed under the MIT License.
- Full text: `ThirdParty/onnxruntime-win-x64-1.17.1/LICENSE`.
- `onnxruntime.dll` is redistributed with the release archives under MIT.

## 5. Kim_Vocal_2.onnx (MDX-Net model) — upstream licence unspecified

- Distributed with Ultimate Vocal Remover (UVR):
  https://github.com/Anjok07/ultimatevocalremovergui
- MDX-Net architecture: KUIELab, https://github.com/kuielab/mdx-net
- **No explicit licence is stated for these weight files upstream.** Some UVR
  model releases are MIT, but that does not automatically cover `Kim_Vocal_2.onnx`.
  Do not assume a licence; if you hold the rights, please contact the project.
- 该文件来自 UVR 生态。**上游未对权重声明明确许可**；部分 UVR 模型为 MIT，但不自动
  覆盖本文件。请勿假定其许可；权利持有人如有异议请联系本项目。

## 6. vocals_weights.bin — reverse-engineered model weights, rights unverified

- Extracted from a commercial `.hance` model format; the format analysis is
  documented in `Tools/hance_format_notes.md`.
- The original weights are **not** authored by this project and no redistribution
  licence has been obtained. This file is provided for study/experimentation only.
- 该权重由商业 `.hance` 模型格式逆向提取（见 `Tools/hance_format_notes.md`），
  原始权重并非本项目创作，未取得再分发授权，仅供学习研究使用。

---

**Note / 注意**: items 5 and 6 are third-party model assets with unclear or
unverified redistribution rights. They are not covered by this project's AGPLv3
grant. Commercial use or redistribution of release archives containing them
requires you to verify and obtain the necessary rights from the respective
holders. This is not legal advice.

第 5、6 项为权利状态不明或未核实的第三方模型资产，不在本项目 AGPLv3 授权范围内。
如需商业使用或再分发含这些资产的发布包，请自行向权利人核实并取得许可。本声明不构成法律意见。
