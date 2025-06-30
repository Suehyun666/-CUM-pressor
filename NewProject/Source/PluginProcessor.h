#pragma once
#include <JuceHeader.h>

class AutoCompressorAudioProcessor : public juce::AudioProcessor
{
public:
    AutoCompressorAudioProcessor();
    ~AutoCompressorAudioProcessor() override;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

#ifndef JucePlugin_PreferredChannelConfigurations
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
#endif

    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;
    const juce::String getName() const override;
    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;
    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram(int index) override;
    const juce::String getProgramName(int index) override;
    void changeProgramName(int index, const juce::String& newName) override;
    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    // UI���� ȣ���� �� �ִ� public �Լ���
    void setAutoCompressionEnabled(bool enabled);
    bool isAutoCompressionEnabled() const;
    void setCompressionLevel(int level); // �� �Լ� �߰�!

    // �Ķ���� ����
    juce::AudioProcessorValueTreeState parameters;

private:
    // �������� ���� ������
    std::atomic<float>* autoCompressEnabled;

    // ���� ������ ������
    float currentRMS = 0.0f;
    float rmsSmoothing = 0.99f;

    // �������� �Ķ���͵�
    float threshold = -20.0f;
    float ratio = 4.0f;
    float attack = 10.0f;
    float release = 100.0f;
    float makeupGain = 0.0f;

    // �������� ���� ������
    float envelope = 0.0f;
    float attackCoeff = 0.0f;
    float releaseCoeff = 0.0f;

    double sampleRate = 44100.0;

    // �ڵ� �м��� ����
    static const int analysisBufferSize = 4096;
    juce::AudioBuffer<float> analysisBuffer;
    int analysisBufferIndex = 0;
    bool needsAnalysis = true;

    // ��ƿ��Ƽ �Լ���
    void updateCompressorCoefficients();
    void analyzeAudioLevel(const juce::AudioBuffer<float>& buffer);
    void calculateAutoParameters();
    float applyCompression(float inputSample);
    float dbToLinear(float db) { return std::pow(10.0f, db / 20.0f); }
    float linearToDb(float linear) { return 20.0f * std::log10(std::max(linear, 1e-6f)); }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AutoCompressorAudioProcessor)
};

// Editor���� ȣȯ���� ���� typedef
using NewProjectAudioProcessor = AutoCompressorAudioProcessor;