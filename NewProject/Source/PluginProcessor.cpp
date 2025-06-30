#include "PluginProcessor.h"
#include "PluginEditor.h"

// 생성자 - 플러그인 초기화 및 파라미터 설정
AutoCompressorAudioProcessor::AutoCompressorAudioProcessor()
#ifndef JucePlugin_PreferredChannelConfigurations
    : AudioProcessor(BusesProperties()
#if ! JucePlugin_IsMidiEffect
#if ! JucePlugin_IsSynth
        .withInput("Input", juce::AudioChannelSet::stereo(), true)   // 스테레오 입력 설정
#endif
        .withOutput("Output", juce::AudioChannelSet::stereo(), true) // 스테레오 출력 설정
#endif
    ),
#endif
    parameters(*this, nullptr, juce::Identifier("AutoCompressor"),
        {
            // Auto Compress 활성화/비활성화 파라미터 생성 (기본값: true)
            std::make_unique<juce::AudioParameterBool>("autoCompress", "Auto Compress", true)
        })
{
    // 파라미터 포인터 연결
    autoCompressEnabled = parameters.getRawParameterValue("autoCompress");

    // 분석 버퍼 초기화 (스테레오, 지정된 크기)
    analysisBuffer.setSize(2, analysisBufferSize);
}

// 소멸자
AutoCompressorAudioProcessor::~AutoCompressorAudioProcessor()
{
}

// 플러그인 이름 반환
const juce::String AutoCompressorAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

// MIDI 입력 허용 여부 확인
bool AutoCompressorAudioProcessor::acceptsMidi() const
{
#if JucePlugin_WantsMidiInput
    return true;
#else
    return false;
#endif
}

// MIDI 출력 생성 여부 확인
bool AutoCompressorAudioProcessor::producesMidi() const
{
#if JucePlugin_ProducesMidiOutput
    return true;
#else
    return false;
#endif
}

// MIDI 이펙트 여부 확인
bool AutoCompressorAudioProcessor::isMidiEffect() const
{
#if JucePlugin_IsMidiEffect
    return true;
#else
    return false;
#endif
}

// 테일 길이 반환 (이펙트가 오디오를 지속하는 시간)
double AutoCompressorAudioProcessor::getTailLengthSeconds() const
{
    return 0.0; // 컴프레서는 테일이 없음
}

// 프로그램 개수 반환
int AutoCompressorAudioProcessor::getNumPrograms()
{
    return 1; // 단일 프로그램만 지원
}

// 현재 프로그램 인덱스 반환
int AutoCompressorAudioProcessor::getCurrentProgram()
{
    return 0;
}

// 프로그램 변경 (사용하지 않음)
void AutoCompressorAudioProcessor::setCurrentProgram(int index)
{
}

// 프로그램 이름 반환
const juce::String AutoCompressorAudioProcessor::getProgramName(int index)
{
    return {};
}

// 프로그램 이름 변경 (사용하지 않음)
void AutoCompressorAudioProcessor::changeProgramName(int index, const juce::String& newName)
{
}

// 오디오 처리 준비 - 샘플레이트와 버퍼 크기 설정
void AutoCompressorAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    this->sampleRate = sampleRate;
    updateCompressorCoefficients(); // 컴프레서 계수 업데이트

    // 분석 버퍼 초기화
    analysisBuffer.clear();
    analysisBufferIndex = 0;
    needsAnalysis = true;

    // 상태 변수 초기화
    envelope = 0.0f;        // 엔벨로프 팔로워 초기화
    currentRMS = 0.0f;      // 현재 RMS 레벨 초기화
}

// 리소스 해제
void AutoCompressorAudioProcessor::releaseResources()
{
}

#ifndef JucePlugin_PreferredChannelConfigurations
// 버스 레이아웃 지원 여부 확인
bool AutoCompressorAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
#if JucePlugin_IsMidiEffect
    juce::ignoreUnused(layouts);
    return true;
#else
    // 모노 또는 스테레오만 지원
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
        && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

#if ! JucePlugin_IsSynth
    // 입력과 출력 채널 수가 동일해야 함
    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;
#endif

    return true;
#endif
}
#endif

// 메인 오디오 처리 함수
void AutoCompressorAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals; // 비정규화된 수 방지
    auto totalNumInputChannels = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();

    // 사용하지 않는 출력 채널 클리어
    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear(i, 0, buffer.getNumSamples());

    // Auto Compress 활성화 상태 확인
    bool autoCompressOn = *autoCompressEnabled > 0.5f;

    if (autoCompressOn)
    {
        // 1. 오디오 레벨 분석
        analyzeAudioLevel(buffer);

        // 2. 분석이 필요하면 자동 파라미터 계산
        if (needsAnalysis)
        {
            calculateAutoParameters();      // 자동 파라미터 계산
            updateCompressorCoefficients(); // 컴프레서 계수 업데이트
            needsAnalysis = false;
        }

        // 3. 각 채널에 컴프레션 적용
        for (int channel = 0; channel < totalNumInputChannels; ++channel)
        {
            auto* channelData = buffer.getWritePointer(channel);

            for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
            {
                channelData[sample] = applyCompression(channelData[sample]);
            }
        }
    }
}

// UI와 연결하기 위한 public 함수들

// 자동 컴프레션 활성화/비활성화 설정
void AutoCompressorAudioProcessor::setAutoCompressionEnabled(bool enabled)
{
    if (auto* param = parameters.getParameter("autoCompress"))
    {
        param->setValueNotifyingHost(enabled ? 1.0f : 0.0f);
    }
}

// 자동 컴프레션 활성화 상태 반환
bool AutoCompressorAudioProcessor::isAutoCompressionEnabled() const
{
    return *autoCompressEnabled > 0.5f;
}

// 오디오 레벨 분석 함수
void AutoCompressorAudioProcessor::analyzeAudioLevel(const juce::AudioBuffer<float>& buffer)
{
    // RMS (Root Mean Square) 레벨 계산
    float sumSquares = 0.0f;
    int totalSamples = 0;

    // 모든 채널의 모든 샘플을 순회하여 제곱합 계산
    for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
    {
        const auto* channelData = buffer.getReadPointer(channel);

        for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
        {
            sumSquares += channelData[sample] * channelData[sample];
            totalSamples++;
        }
    }

    // RMS 계산 및 스무딩 적용
    float rms = std::sqrt(sumSquares / totalSamples);
    currentRMS = rmsSmoothing * currentRMS + (1.0f - rmsSmoothing) * rms;

    // 분석 버퍼에 데이터 저장 (순환 버퍼 방식)
    for (int channel = 0; channel < std::min(buffer.getNumChannels(), analysisBuffer.getNumChannels()); ++channel)
    {
        const auto* sourceData = buffer.getReadPointer(channel);
        auto* destData = analysisBuffer.getWritePointer(channel);

        for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
        {
            destData[analysisBufferIndex] = sourceData[sample];
            analysisBufferIndex = (analysisBufferIndex + 1) % analysisBufferSize;

            // 버퍼가 가득 찼을 때 분석 플래그 설정
            if (analysisBufferIndex == 0)
                needsAnalysis = true;
        }
    }
}

// 현재 RMS 레벨에 따른 자동 파라미터 계산
void AutoCompressorAudioProcessor::calculateAutoParameters()
{
    // 현재 RMS 레벨을 dB로 변환
    float rmsDb = linearToDb(currentRMS);

    // RMS 레벨에 따른 컴프레서 파라미터 자동 설정
    if (rmsDb < -60.0f)         // 매우 조용한 신호 (-60dB 이하)
    {
        threshold = -40.0f;     // 임계값
        ratio = 2.0f;           // 압축비
        attack = 20.0f;         // 어택 시간 (ms)
        release = 200.0f;       // 릴리스 시간 (ms)
        makeupGain = 6.0f;      // 메이크업 게인 (dB)
    }
    else if (rmsDb < -40.0f)    // 조용한 신호 (-40dB ~ -60dB)
    {
        threshold = -30.0f;
        ratio = 3.0f;
        attack = 15.0f;
        release = 150.0f;
        makeupGain = 4.0f;
    }
    else if (rmsDb < -20.0f)    // 보통 신호 (-20dB ~ -40dB)
    {
        threshold = -20.0f;
        ratio = 4.0f;
        attack = 10.0f;
        release = 100.0f;
        makeupGain = 2.0f;
    }
    else if (rmsDb < -10.0f)    // 큰 신호 (-10dB ~ -20dB)
    {
        threshold = -15.0f;
        ratio = 6.0f;
        attack = 5.0f;
        release = 80.0f;
        makeupGain = 1.0f;
    }
    else                        // 매우 큰 신호 (-10dB 이상)
    {
        threshold = -10.0f;
        ratio = 8.0f;
        attack = 2.0f;
        release = 50.0f;
        makeupGain = 0.0f;
    }
}

// 컴프레서 계수 업데이트 (어택/릴리스 시간 상수 계산)
void AutoCompressorAudioProcessor::updateCompressorCoefficients()
{
    // 어택/릴리스 시간을 1차 필터 계수로 변환
    attackCoeff = std::exp(-1.0f / (attack * 0.001f * sampleRate));
    releaseCoeff = std::exp(-1.0f / (release * 0.001f * sampleRate));
}

// 단일 샘플에 컴프레션 적용
float AutoCompressorAudioProcessor::applyCompression(float inputSample)
{
    // 입력 레벨 계산 (절댓값)
    float inputLevel = std::abs(inputSample);
    float inputDb = linearToDb(inputLevel);

    // 컴프레션 계산
    float compressedDb = inputDb;
    if (inputDb > threshold)    // 임계값을 초과하는 경우만 압축
    {
        float excess = inputDb - threshold;         // 초과분 계산
        compressedDb = threshold + excess / ratio;  // 압축비 적용
    }

    // 게인 리덕션 계산
    float gainReductionDb = compressedDb - inputDb;
    float targetGain = dbToLinear(gainReductionDb);

    // 엔벨로프 팔로워 (부드러운 게인 변화를 위한 1차 필터)
    if (targetGain < envelope)      // 어택 (게인 감소)
        envelope = targetGain + (envelope - targetGain) * attackCoeff;
    else                            // 릴리스 (게인 증가)
        envelope = targetGain + (envelope - targetGain) * releaseCoeff;

    // 메이크업 게인 적용
    float makeupLinear = dbToLinear(makeupGain);

    // 최종 출력 = 입력 * 엔벨로프 * 메이크업 게인
    return inputSample * envelope * makeupLinear;
}

// 에디터 존재 여부 반환
bool AutoCompressorAudioProcessor::hasEditor() const
{
    return true;
}

// 에디터 생성
juce::AudioProcessorEditor* AutoCompressorAudioProcessor::createEditor()
{
    return new NewProjectAudioProcessorEditor(*this);
}

// 플러그인 상태 저장
void AutoCompressorAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    auto state = parameters.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);
}

// 플러그인 상태 복원
void AutoCompressorAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState(getXmlFromBinary(data, sizeInBytes));

    if (xmlState.get() != nullptr)
        if (xmlState->hasTagName(parameters.state.getType()))
            parameters.replaceState(juce::ValueTree::fromXml(*xmlState));
}

// 수동 컴프레션 레벨 설정 (0-4단계)
void AutoCompressorAudioProcessor::setCompressionLevel(int level)
{
    switch (level)
    {
    case 0: // 1단계 - 매우 부드러운 압축
        threshold = -30.0f;
        ratio = 1.5f;
        attack = 30.0f;
        release = 200.0f;
        makeupGain = 1.0f;
        break;

    case 1: // 2단계 - 부드러운 압축
        threshold = -25.0f;
        ratio = 2.5f;
        attack = 20.0f;
        release = 150.0f;
        makeupGain = 2.0f;
        break;

    case 2: // 3단계 - 보통 압축 (기본값)
        threshold = -20.0f;
        ratio = 4.0f;
        attack = 10.0f;
        release = 100.0f;
        makeupGain = 3.0f;
        break;

    case 3: // 4단계 - 강한 압축
        threshold = -15.0f;
        ratio = 6.0f;
        attack = 5.0f;
        release = 80.0f;
        makeupGain = 4.0f;
        break;

    case 4: // 5단계 - 매우 강한 압축
        threshold = -10.0f;
        ratio = 8.0f;
        attack = 2.0f;
        release = 50.0f;
        makeupGain = 5.0f;
        break;
    }

    // 새로운 설정으로 계수 업데이트
    updateCompressorCoefficients();
}

// 플러그인 인스턴스 생성 함수
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new AutoCompressorAudioProcessor();
}