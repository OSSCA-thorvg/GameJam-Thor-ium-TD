project('thorium-td', 'cpp', version: '0.1.0', default_options: ['cpp_std=c++17', 'warning_level=2'])

content_dir = host_machine.system() == 'emscripten' ? '/content' : meson.project_source_root() / 'content'
add_project_arguments('-DTHORIUM_CONTENT_DIR="' + content_dir + '"', language: 'cpp')
# Fused multiply-add changes float results between targets; keeping it off makes simulations match everywhere.
add_project_arguments(meson.get_compiler('cpp').get_supported_arguments('-ffp-contract=off'), language: 'cpp')

thorvg_dep = subproject('thorvg', default_options: [
  'default_library=static', 'static=true', 'engines=cpu', 'loaders=lottie,ttf,svg',
  'threads=false', 'extra=', 'tools=', 'savers=', 'tests=false', 'partial=false',
]).get_variable('thorvg_dep')
sdl_dep = dependency('SDL3', method: 'cmake', modules: ['SDL3::SDL3-static'], static: true)
platform_deps = []
if host_machine.system() == 'darwin'
  # Meson's CMake importer cannot expand SDL's LINK_LIBRARY:FRAMEWORK expressions.
  platform_deps += dependency('appleframeworks', modules: ['CoreMedia', 'CoreVideo', 'Cocoa', 'IOKit', 'Carbon', 'Foundation', 'Metal', 'QuartzCore', 'UniformTypeIdentifiers', 'AudioToolbox', 'CoreAudio', 'AVFoundation'])
endif
inc = include_directories('src')
profiling = static_library('thorium-profiling', 'src/engine/diagnostics/FrameProfiler.cpp', include_directories: inc)

rules = static_library('thorium-rules', 'src/game/rules/Run.cpp', 'src/game/rules/Combat.cpp', 'src/game/rules/Cards.cpp', 'src/game/rules/Mining.cpp', 'src/game/rules/Progression.cpp', 'src/game/rules/RunSnapshot.cpp', 'src/game/content/Catalog.cpp', 'src/game/content/StageCatalog.cpp', 'src/game/content/MineCatalog.cpp', 'src/game/content/Excavation.cpp', 'src/game/content/TutorialCatalog.cpp', 'src/game/content/CatalogValidation.cpp', include_directories: inc)
animation = static_library('thorium-animation', 'src/engine/animation/Animation.cpp', include_directories: inc)
ui = static_library('thorium-ui', 'src/engine/ui/Ui.cpp', include_directories: inc)
soundscape = static_library('thorium-soundscape', 'src/game/presentation/Soundscape.cpp', include_directories: inc)
feedback = static_library('thorium-feedback', 'src/game/presentation/Feedback.cpp', 'src/game/presentation/ActorAnimator.cpp', 'src/game/presentation/AnimationBindings.cpp', include_directories: inc)
graphics = static_library('thorium-graphics', 'src/engine/render/Renderer.cpp', 'src/engine/render/AnimationRendering.cpp', 'src/engine/render/AnimationCache.cpp', 'src/engine/render/AnimationFrameBaker.cpp', dependencies: thorvg_dep, include_directories: [inc, include_directories('subprojects/thorvg/src/loaders/lottie')])
audio = static_library('thorium-audio', 'src/engine/audio/Audio.cpp', 'src/engine/audio/Mp3.cpp', include_directories: [inc, include_directories('third_party/dr_mp3')])

text = static_library('thorium-text', 'src/engine/text/Strings.cpp', 'src/game/presentation/Text.cpp', include_directories: inc)

controllers = static_library('thorium-controllers', 'src/app/AppFlow.cpp', 'src/app/MapCamera.cpp', 'src/app/MineScroll.cpp', 'src/app/GameplayController.cpp', 'src/app/FeedbackChannel.cpp', 'src/app/TutorialDirector.cpp', 'src/game/presentation/Layout.cpp', include_directories: inc, link_with: text)

client = static_library('thorium-client', 'src/app/Application.cpp', 'src/app/BenchScene.cpp', 'src/app/Persistence.cpp', 'src/game/presentation/View.cpp', 'src/game/presentation/Placement.cpp', 'src/game/presentation/CardDetails.cpp', 'src/game/presentation/CardUi.cpp', 'src/game/presentation/GameUi.cpp', 'src/game/presentation/MineUi.cpp', 'src/game/presentation/ExcavationUi.cpp', 'src/game/presentation/CombatFeedback.cpp', 'src/game/presentation/EnemyView.cpp', 'src/game/presentation/Battlefield.cpp', 'src/game/presentation/FrontEnd.cpp', 'src/game/presentation/TutorialUi.cpp', include_directories: inc, link_with: text)
sdl_output = static_library('thorium-sdl-output', 'src/platform/sdl/AudioDevice.cpp', 'src/platform/sdl/VideoOutput.cpp', include_directories: inc, dependencies: sdl_dep)
client_sources = files('src/platform/sdl/main.cpp')
client_libraries = [client, sdl_output, controllers, rules, ui, graphics, feedback, animation, audio, soundscape, profiling]
client_links = []
suffix = ''
if host_machine.system() == 'emscripten'
  content_index = run_command(find_program('python3'), files('tools/list_content.py'), check: true).stdout().strip().split('\n')
  asset_config = configuration_data()
  asset_config.set_quoted('THORIUM_CONTENT_SIGNATURE', content_index[0])
  content_stamp = configure_file(output: 'content-signature.h', configuration: asset_config)
  content_assets = [content_stamp]
  foreach index : range(1, content_index.length())
    content_assets += content_index[index]
  endforeach
  suffix = 'js'
  client_links = [
    '-sALLOW_MEMORY_GROWTH=1', '-sINITIAL_MEMORY=67108864', '-sSTACK_SIZE=2097152',
    '-sENVIRONMENT=web', '-sASSERTIONS=1', '-sEXIT_RUNTIME=1',
    '-sEXPORTED_FUNCTIONS=_main,_thorium_debug_state,_thorium_debug_ui,_thorium_bench_frames',
    '-sEXPORTED_RUNTIME_METHODS=UTF8ToString',
    '--preload-file', meson.project_source_root() / 'content' + '@/content',
  ]
endif

if suffix == 'js'
  app = executable('thorium', client_sources, include_directories: inc, link_with: client_libraries, dependencies: [thorvg_dep, sdl_dep] + platform_deps, link_args: client_links, name_suffix: 'js', link_depends: content_assets)
else
  app = executable('thorium', client_sources, include_directories: inc, link_with: client_libraries, dependencies: [thorvg_dep, sdl_dep] + platform_deps)
  executable('thorium-combat-benchmark', 'benchmarks/combat_benchmark.cpp', include_directories: inc, link_with: [rules, profiling])
  executable('thorium-frame-benchmark', 'benchmarks/frame_benchmark.cpp', include_directories: inc, link_with: client_libraries, dependencies: [thorvg_dep, sdl_dep] + platform_deps)
  application_tests = executable('thorium-application-tests', 'tests/native/application_test.cpp', include_directories: inc, link_with: [client, controllers, rules, ui, graphics, feedback, animation, audio, soundscape, profiling], dependencies: thorvg_dep)
  test('application-content-and-pixels', application_tests)
  profiler_tests = executable('thorium-profiler-tests', 'tests/native/frame_profiler_test.cpp', include_directories: inc, link_with: profiling)
  test('frame-profiling', profiler_tests)
  validation_tests = executable('thorium-catalog-validation-tests', 'tests/native/catalog_validation_test.cpp', include_directories: inc, link_with: rules)
  test('catalog-validation', validation_tests)
  offer_tests = executable('thorium-card-offer-tests', 'tests/native/card_offers_test.cpp', include_directories: inc, link_with: rules)
  test('card-offer-authority', offer_tests)
  preview_tests = executable('thorium-play-preview-tests', 'tests/native/play_preview_test.cpp', include_directories: inc, link_with: rules)
  test('play-preview-and-commit', preview_tests)
  rule_tests = executable('thorium-tests', 'tests/native/rules_test.cpp', include_directories: inc, link_with: [rules, ui, feedback, animation])
  test('rules-and-input', rule_tests)
  audio_tests = executable('thorium-audio-tests', 'tests/native/audio_test.cpp', include_directories: inc, link_with: [audio, soundscape, rules])
  test('audio-and-sound-routing', audio_tests, args: [meson.project_source_root() / 'tests/fixtures/audio'])
  playthrough_tests = executable('thorium-campaign-playthrough-tests', 'tests/native/campaign_playthrough_test.cpp', include_directories: inc, link_with: rules)
  test('campaign-real-cost-playthrough', playthrough_tests)
  mine_tests = executable('thorium-mine-content-tests', 'tests/native/mine_content_test.cpp', include_directories: inc, link_with: rules)
  test('mine-special-tiles', mine_tests)
  snapshot_tests = executable('thorium-run-snapshot-tests', 'tests/native/run_snapshot_test.cpp', include_directories: inc, link_with: rules)
  test('run-snapshot-and-restore', snapshot_tests, timeout: 120)
  excavation_tests = executable('thorium-excavation-tests', 'tests/native/excavation_test.cpp', include_directories: inc, link_with: rules)
  test('excavation-walls-and-strikes', excavation_tests)
  economy_tests = executable('thorium-economy-sink-tests', 'tests/native/economy_sinks_test.cpp', include_directories: inc, link_with: rules)
  test('economy-sinks', economy_tests)
  balance_harness = executable('thorium-balance-harness', 'tests/native/balance_harness.cpp', include_directories: inc, link_with: rules)
  test('balance-curve', balance_harness, args: ['--seeds', '1-16', '--check'], timeout: 120)
  test('balance-curve-excavation', balance_harness, args: ['--seeds', '1-16', '--mining', 'excavate', '--check'], timeout: 120)
  tower_card_tests = executable('thorium-tower-card-tests', 'tests/native/tower_cards_test.cpp', include_directories: inc, link_with: rules)
  test('tower-card-rules', tower_card_tests)
  roster_tests = executable('thorium-enemy-roster-tests', 'tests/native/enemy_roster_test.cpp', include_directories: inc, link_with: rules)
  test('enemy-roster-mechanics', roster_tests)
  gimmick_tests = executable('thorium-stage-gimmick-tests', 'tests/native/stage_gimmicks_test.cpp', include_directories: inc, link_with: rules)
  test('stage-gimmicks', gimmick_tests)
  machine_tests = executable('thorium-machine-and-role-tests', 'tests/native/machines_and_roles_test.cpp', include_directories: inc, link_with: rules)
  test('machines-and-new-roles', machine_tests)
  campaign_tests = executable('thorium-campaign-tests', 'tests/native/campaign_test.cpp', include_directories: inc, link_with: [controllers, rules, ui, feedback, animation, audio, soundscape])
  test('campaign-progression-and-camera', campaign_tests)
  controller_tests = executable('thorium-controller-tests', 'tests/native/controllers_test.cpp', include_directories: inc, link_with: [controllers, rules, ui, feedback, animation, audio, soundscape])
  test('controllers-and-content', controller_tests)
  tutorial_tests = executable('thorium-tutorial-tests', 'tests/native/tutorial_test.cpp', include_directories: inc, link_with: [controllers, rules, ui, feedback, animation, audio, soundscape])
  test('tutorial-progression-and-isolation', tutorial_tests)
  asset_tests = executable('thorium-render-asset-tests', 'tests/native/render_assets_test.cpp', include_directories: inc, link_with: [graphics, animation], dependencies: thorvg_dep)
  test('render-asset-boundary', asset_tests, args: [meson.project_build_root() / 'animation-preview.rgba'])
  animation_tests = executable('thorium-animation-tests', 'tests/native/animation_test.cpp', include_directories: inc, link_with: [rules, feedback, animation])
  test('animation-and-actor-lifecycle', animation_tests)
  executable('thorium-animation-benchmark', 'benchmarks/animation_benchmark.cpp', include_directories: inc, link_with: [graphics, animation], dependencies: thorvg_dep)
  cache_tests = executable('thorium-animation-cache-tests', 'tests/native/animation_cache_test.cpp', include_directories: inc, link_with: [graphics, animation, feedback, rules], dependencies: thorvg_dep)
  test('shared-animation-cache', cache_tests)
  i18n_tests = executable('thorium-i18n-tests', 'tests/native/i18n_test.cpp', include_directories: inc, link_with: text)
  test('i18n-tables', i18n_tests, args: [meson.project_source_root() / 'content/i18n'])
  test('native-render-smoke', app, args: ['--smoke-test'], env: ['SDL_VIDEODRIVER=dummy', 'SDL_RENDER_DRIVER=software', 'SDL_AUDIODRIVER=dummy'])
endif
