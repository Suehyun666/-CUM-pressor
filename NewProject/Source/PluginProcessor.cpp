#include "PluginProcessor.h"
#include "PluginEditor.h"

// ������ - �÷����� �ʱ�ȭ �� �Ķ���� ����
AutoCompressorAudioProcessor::AutoCompressorAudioProcessor()
#ifndef JucePlugin_PreferredChannelConfigurations
    : AudioProcessor(BusesProperties()
#if ! JucePlugin_IsMidiEffect
#if ! JucePlugin_IsSynth
        .withInput("Input", juce::AudioChannelSet::stereo(), true)   // ���׷��� �Է� ����
#endif
        .withOutput("Output", juce::AudioChannelSet::stereo(), true) // ���׷��� ��� ����
#endif
    ),
#endif
    parameters(*this, nullptr, juce::Identifier("AutoCompressor"),
        {
            // Auto Compress Ȱ��ȭ/��Ȱ��ȭ �Ķ���� ���� (�⺻��: true)
            std::make_unique<juce::AudioParameterBool>("autoCompress", "Auto Compress", true)
        })
{
    // �Ķ���� ������ ����
    autoCompressEnabled = parameters.getRawParameterValue("autoCompress");

    // �м� ���� �ʱ�ȭ (���׷���, ������ ũ��)
    analysisBuffer.setSize(2, analysisBufferSize);
}

// �Ҹ���
AutoCompressorAudioProcessor::~AutoCompressorAudioProcessor()
{
}

// �÷����� �̸� ��ȯ
const juce::String AutoCompressorAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

// MIDI �Է� ��� ���� Ȯ��
bool AutoCompressorAudioProcessor::acceptsMidi() const
{
#if JucePlugin_WantsMidiInput
    return true;
#else
    return false;
#endif
}

// MIDI ��� ���� ���� Ȯ��
bool AutoCompressorAudioProcessor::producesMidi() const
{
#if JucePlugin_ProducesMidiOutput
    return true;
#else
    return false;
#endif
}

// MIDI ����Ʈ ���� Ȯ��
bool AutoCompressorAudioProcessor::isMidiEffect() const
{
#if JucePlugin_IsMidiEffect
    return true;
#else
    return false;
#endif
}

// ���� ���� ��ȯ (����Ʈ�� ������� �����ϴ� �ð�)
double AutoCompressorAudioProcessor::getTailLengthSeconds() const
{
    return 0.0; // ���������� ������ ����
}

// ���α׷� ���� ��ȯ
int AutoCompressorAudioProcessor::getNumPrograms()
{
    return 1; // ���� ���α׷��� ����
}

// ���� ���α׷� �ε��� ��ȯ
int AutoCompressorAudioProcessor::getCurrentProgram()
{
    return 0;
}

// ���α׷� ���� (������� ����)
void AutoCompressorAudioProcessor::setCurrentProgram(int index)
{
}

// ���α׷� �̸� ��ȯ
const juce::String AutoCompressorAudioProcessor::getProgramName(int index)
{
    return {};
}

// ���α׷� �̸� ���� (������� ����)
void AutoCompressorAudioProcessor::changeProgramName(int index, const juce::String& newName)
{
}

// ����� ó�� �غ� - ���÷���Ʈ�� ���� ũ�� ����
void AutoCompressorAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    this->sampleRate = sampleRate;
    updateCompressorCoefficients(); // �������� ��� ������Ʈ

    // �м� ���� �ʱ�ȭ
    analysisBuffer.clear();
    analysisBufferIndex = 0;
    needsAnalysis = true;

    // ���� ���� �ʱ�ȭ
    envelope = 0.0f;        // �������� �ȷο� �ʱ�ȭ
    currentRMS = 0.0f;      // ���� RMS ���� �ʱ�ȭ
}

// ���ҽ� ����
void AutoCompressorAudioProcessor::releaseResources()
{
}

#ifndef JucePlugin_PreferredChannelConfigurations
// ���� ���̾ƿ� ���� ���� Ȯ��
bool AutoCompressorAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
#if JucePlugin_IsMidiEffect
    juce::ignoreUnused(layouts);
    return true;
#else
    // ��� �Ǵ� ���׷����� ����
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
        && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

#if ! JucePlugin_IsSynth
    // �Է°� ��� ä�� ���� �����ؾ� ��
    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;
#endif

    return true;
#endif
}
#endif

// ���� ����� ó�� �Լ�
void AutoCompressorAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals; // ������ȭ�� �� ����
    auto totalNumInputChannels = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();

    // ������� �ʴ� ��� ä�� Ŭ����
    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear(i, 0, buffer.getNumSamples());

    // Auto Compress Ȱ��ȭ ���� Ȯ��
    bool autoCompressOn = *autoCompressEnabled > 0.5f;

    if (autoCompressOn)
    {
        // 1. ����� ���� �м�
        analyzeAudioLevel(buffer);

        // 2. �м��� �ʿ��ϸ� �ڵ� �Ķ���� ���
        if (needsAnalysis)
        {
            calculateAutoParameters();      // �ڵ� �Ķ���� ���
            updateCompressorCoefficients(); // �������� ��� ������Ʈ
            needsAnalysis = false;
        }

        // 3. �� ä�ο� �������� ����
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

// UI�� �����ϱ� ���� public �Լ���

// �ڵ� �������� Ȱ��ȭ/��Ȱ��ȭ ����
void AutoCompressorAudioProcessor::setAutoCompressionEnabled(bool enabled)
{
    if (auto* param = parameters.getParameter("autoCompress"))
    {
        param->setValueNotifyingHost(enabled ? 1.0f : 0.0f);
    }
}

// �ڵ� �������� Ȱ��ȭ ���� ��ȯ
bool AutoCompressorAudioProcessor::isAutoCompressionEnabled() const
{
    return *autoCompressEnabled > 0.5f;
}

// ����� ���� �м� �Լ�
void AutoCompressorAudioProcessor::analyzeAudioLevel(const juce::AudioBuffer<float>& buffer)
{
    // RMS (Root Mean Square) ���� ���
    float sumSquares = 0.0f;
    int totalSamples = 0;

    // ��� ä���� ��� ������ ��ȸ�Ͽ� ������ ���
    for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
    {
        const auto* channelData = buffer.getReadPointer(channel);

        for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
        {
            sumSquares += channelData[sample] * channelData[sample];
            totalSamples++;
        }
    }

    // RMS ��� �� ������ ����
    float rms = std::sqrt(sumSquares / totalSamples);
    currentRMS = rmsSmoothing * currentRMS + (1.0f - rmsSmoothing) * rms;

    // �м� ���ۿ� ������ ���� (��ȯ ���� ���)
    for (int channel = 0; channel < std::min(buffer.getNumChannels(), analysisBuffer.getNumChannels()); ++channel)
    {
        const auto* sourceData = buffer.getReadPointer(channel);
        auto* destData = analysisBuffer.getWritePointer(channel);

        for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
        {
            destData[analysisBufferIndex] = sourceData[sample];
            analysisBufferIndex = (analysisBufferIndex + 1) % analysisBufferSize;

            // ���۰� ���� á�� �� �м� �÷��� ����
            if (analysisBufferIndex == 0)
                needsAnalysis = true;
        }
    }
}

// ���� RMS ������ ���� �ڵ� �Ķ���� ���
void AutoCompressorAudioProcessor::calculateAutoParameters()
{
    // ���� RMS ������ dB�� ��ȯ
    float rmsDb = linearToDb(currentRMS);

    // RMS ������ ���� �������� �Ķ���� �ڵ� ����
    if (rmsDb < -60.0f)         // �ſ� ������ ��ȣ (-60dB ����)
    {
        threshold = -40.0f;     // �Ӱ谪
        ratio = 2.0f;           // �����
        attack = 20.0f;         // ���� �ð� (ms)
        release = 200.0f;       // ������ �ð� (ms)
        makeupGain = 6.0f;      // ����ũ�� ���� (dB)
    }
    else if (rmsDb < -40.0f)    // ������ ��ȣ (-40dB ~ -60dB)
    {
        threshold = -30.0f;
        ratio = 3.0f;
        attack = 15.0f;
        release = 150.0f;
        makeupGain = 4.0f;
    }
    else if (rmsDb < -20.0f)    // ���� ��ȣ (-20dB ~ -40dB)
    {
        threshold = -20.0f;
        ratio = 4.0f;
        attack = 10.0f;
        release = 100.0f;
        makeupGain = 2.0f;
    }
    else if (rmsDb < -10.0f)    // ū ��ȣ (-10dB ~ -20dB)
    {
        threshold = -15.0f;
        ratio = 6.0f;
        attack = 5.0f;
        release = 80.0f;
        makeupGain = 1.0f;
    }
    else                        // �ſ� ū ��ȣ (-10dB �̻�)
    {
        threshold = -10.0f;
        ratio = 8.0f;
        attack = 2.0f;
        release = 50.0f;
        makeupGain = 0.0f;
    }
}

// �������� ��� ������Ʈ (����/������ �ð� ��� ���)
void AutoCompressorAudioProcessor::updateCompressorCoefficients()
{
    // ����/������ �ð��� 1�� ���� ����� ��ȯ
    attackCoeff = std::exp(-1.0f / (attack * 0.001f * sampleRate));
    releaseCoeff = std::exp(-1.0f / (release * 0.001f * sampleRate));
}

// ���� ���ÿ� �������� ����
float AutoCompressorAudioProcessor::applyCompression(float inputSample)
{
    // �Է� ���� ��� (����)
    float inputLevel = std::abs(inputSample);
    float inputDb = linearToDb(inputLevel);

    // �������� ���
    float compressedDb = inputDb;
    if (inputDb > threshold)    // �Ӱ谪�� �ʰ��ϴ� ��츸 ����
    {
        float excess = inputDb - threshold;         // �ʰ��� ���
        compressedDb = threshold + excess / ratio;  // ����� ����
    }

    // ���� ������ ���
    float gainReductionDb = compressedDb - inputDb;
    float targetGain = dbToLinear(gainReductionDb);

    // �������� �ȷο� (�ε巯�� ���� ��ȭ�� ���� 1�� ����)
    if (targetGain < envelope)      // ���� (���� ����)
        envelope = targetGain + (envelope - targetGain) * attackCoeff;
    else                            // ������ (���� ����)
        envelope = targetGain + (envelope - targetGain) * releaseCoeff;

    // ����ũ�� ���� ����
    float makeupLinear = dbToLinear(makeupGain);

    // ���� ��� = �Է� * �������� * ����ũ�� ����
    return inputSample * envelope * makeupLinear;
}

// ������ ���� ���� ��ȯ
bool AutoCompressorAudioProcessor::hasEditor() const
{
    return true;
}

// ������ ����
juce::AudioProcessorEditor* AutoCompressorAudioProcessor::createEditor()
{
    return new NewProjectAudioProcessorEditor(*this);
}

// �÷����� ���� ����
void AutoCompressorAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    auto state = parameters.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);
}

// �÷����� ���� ����
void AutoCompressorAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState(getXmlFromBinary(data, sizeInBytes));

    if (xmlState.get() != nullptr)
        if (xmlState->hasTagName(parameters.state.getType()))
            parameters.replaceState(juce::ValueTree::fromXml(*xmlState));
}

// ���� �������� ���� ���� (0-4�ܰ�)
void AutoCompressorAudioProcessor::setCompressionLevel(int level)
{
    switch (level)
    {
    case 0: // 1�ܰ� - �ſ� �ε巯�� ����
        threshold = -30.0f;
        ratio = 1.5f;
        attack = 30.0f;
        release = 200.0f;
        makeupGain = 1.0f;
        break;

    case 1: // 2�ܰ� - �ε巯�� ����
        threshold = -25.0f;
        ratio = 2.5f;
        attack = 20.0f;
        release = 150.0f;
        makeupGain = 2.0f;
        break;

    case 2: // 3�ܰ� - ���� ���� (�⺻��)
        threshold = -20.0f;
        ratio = 4.0f;
        attack = 10.0f;
        release = 100.0f;
        makeupGain = 3.0f;
        break;

    case 3: // 4�ܰ� - ���� ����
        threshold = -15.0f;
        ratio = 6.0f;
        attack = 5.0f;
        release = 80.0f;
        makeupGain = 4.0f;
        break;

    case 4: // 5�ܰ� - �ſ� ���� ����
        threshold = -10.0f;
        ratio = 8.0f;
        attack = 2.0f;
        release = 50.0f;
        makeupGain = 5.0f;
        break;
    }

    // ���ο� �������� ��� ������Ʈ
    updateCompressorCoefficients();
}

// �÷����� �ν��Ͻ� ���� �Լ�
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new AutoCompressorAudioProcessor();
}