Pod::Spec.new do |s|
  s.name         = 'HeartPy'
  s.version      = '0.1.0'
  s.summary      = 'React Native bindings for HeartPy-like C++ core'
  s.license      = { :type => 'MIT' }
  s.authors      = { 'you' => 'you@example.com' }
  s.homepage     = 'https://example.com'
  s.platforms    = { :ios => '12.0' }
  s.source       = { :path => '.' }
  # Use the simplified module for stable builds
  s.source_files = 'HeartPyModule.{h,mm}', 'heartpy_core.{h,cpp}', 'heartpy_stream.{h,cpp}', 'rn_options_builder.{h,cpp}', 'kissfft/*.{c,h}'
  s.public_header_files = 'HeartPyModule.h'
  s.requires_arc = true
  s.dependency 'React-Core'
  s.dependency 'React-jsi'
  s.libraries = 'c++'
  s.pod_target_xcconfig = {
    'GCC_PREPROCESSOR_DEFINITIONS' => 'USE_KISSFFT=1',
    'CLANG_CXX_LANGUAGE_STANDARD' => 'c++20',
    'CLANG_CXX_LIBRARY' => 'libc++',
    'HEADER_SEARCH_PATHS' => '"$(PODS_TARGET_SRCROOT)" "$(PODS_TARGET_SRCROOT)/kissfft"'
  }
end

