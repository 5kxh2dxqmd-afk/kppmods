// Interactive examples use the real toolkit, including its state and validation.
import { useRef, useState } from 'react';
import { View as Layout } from 'react-native';
import {
  Button,
  Text,
  Slider,
  TextField,
  Checkbox,
  Switch,
  SettingsToggle,
  SettingsRow,
  ProgressBar,
  ScrollBar,
  ActivityIndicator,
  Divider,
  Image,
  MarkupText,
  QRCode,
  ScrollView,
  RadioGroup,
  TabBar,
  Dialog,
  BottomSheet,
  Overlay,
  StarRating,
  AnimatedText,
  Notebook,
  Page,
  TextMeasurement,
  StaticDocument,
  useKindle
} from 'react-kindle';

const icon =
  'data:image/svg+xml,' +
  encodeURIComponent(
    '<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 64 64"><rect x="8" y="8" width="20" height="20" rx="4"/><rect x="36" y="8" width="20" height="20" rx="4"/><rect x="8" y="36" width="20" height="20" rx="4"/><rect x="36" y="36" width="20" height="20" rx="4"/></svg>'
  );
const row = { flexDirection: 'row', alignItems: 'center', gap: 16, marginVertical: 12 };
function Section({ title, children }) {
  return (
    <Layout style={{ marginBottom: 30 }}>
      <Text variant="title" style={{ marginBottom: 16 }}>
        {title}
      </Text>
      {children}
    </Layout>
  );
}
export default function Gallery() {
  const [value, setValue] = useState(4),
    [enabled, setEnabled] = useState(true);
  const [text, setText] = useState('Evening reading'),
    [surface, setSurface] = useState('');
  const [tab, setTab] = useState('controls'),
    [choice, setChoice] = useState('warm');
  const [clicks, setClicks] = useState(0),
    [page, setPage] = useState('one');
  const input = useRef(null),
    scroll = useRef(null);
  const kindle = useKindle();
  return (
    <ScrollView
      mode="controlled"
      ref={scroll}
      style={{ flex: 1 }}
      contentContainerStyle={{ padding: 28 }}
    >
      <Text size={30} variant="title">
        Made for a quieter screen.
      </Text>
      <Text variant="caption" style={{ marginVertical: 12 }}>
        Real toolkit components · simulated native controls
      </Text>
      <TabBar
        tabs={[
          { value: 'controls', label: 'Controls' },
          { value: 'content', label: 'Content' },
          { value: 'surfaces', label: 'Surfaces' }
        ]}
        value={tab}
        onValueChange={setTab}
      />
      <Layout style={{ height: 24 }} />
      {tab === 'controls' && (
        <>
          <Section title="Buttons & state">
            <Button onPress={() => setClicks((value) => value + 1)}>
              Start reading
            </Button>
            <Text style={{ marginVertical: 12 }}>Pressed {clicks} times</Text>
            <Layout style={row}>
              <Button variant="secondary" onPress={() => setClicks(0)}>
                Reset
              </Button>
              <Button disabled>Unavailable</Button>
            </Layout>
          </Section>
          <Section title="Reading preferences">
            <SettingsRow title="Warm light" description="An interactive setting">
              <Switch
                accessibilityLabel="Warm light"
                value={enabled}
                onValueChange={setEnabled}
              />
            </SettingsRow>
            <Layout style={row}>
              <Checkbox
                accessibilityLabel="Remember preference"
                value={enabled}
                onValueChange={setEnabled}
              />
              <Text>Remember my preference</Text>
              <SettingsToggle value={enabled} onValueChange={setEnabled} />
            </Layout>
            <Text>Intensity · {value}</Text>
            <Slider
              value={value}
              minimumValue={0}
              maximumValue={10}
              onValueChange={setValue}
            />
            <ProgressBar value={value * 10} />
          </Section>
          <Section title="Text & selection">
            <TextField
              ref={input}
              label="Collection name"
              value={text}
              onChangeText={setText}
              hint="Changes stay in React state"
            />
            <Layout style={{ marginTop: 12 }}>
              <Button variant="secondary" onPress={() => input.current.focus()}>
                Focus field
              </Button>
            </Layout>
            <RadioGroup
              value={choice}
              onValueChange={setChoice}
              options={[
                { value: 'warm', label: 'Warm paper' },
                { value: 'cool', label: 'Cool paper' }
              ]}
            />
          </Section>
        </>
      )}
      {tab === 'content' && (
        <>
          <Section title="Typography">
            <Text variant="h2">A library in your pocket</Text>
            <Text variant="body2">
              The same component source runs here and on your Kindle.
            </Text>
            <StarRating value={3.5} />
            <AnimatedText>Preparing your book</AnimatedText>
          </Section>
          <Section title="Images & codes">
            <Layout style={row}>
              <Image source={icon} style={{ width: 64, height: 64 }} />
              <QRCode value="https://standardebooks.org" size={132} />
            </Layout>
            <MarkupText
              markup={
                '<span font_desc="sans 20px">Pango <b>bold</b> and <i>italic</i> preview</span>'
              }
            />
          </Section>
          <Section title="Documents">
            <StaticDocument
              source={
                'data:text/html,' +
                encodeURIComponent(
                  '<html><body style="font:20px Georgia;padding:16px"><h2>A small document</h2><p>HTML rendered by the browser. Native Kindle layout may differ.</p></body></html>'
                )
              }
              style={{ height: 200, width: '100%' }}
            />
            <TextMeasurement width={300} height={60}>
              <Text>A bounded text region.</Text>
            </TextMeasurement>
          </Section>
          <Section title="Progress">
            <ActivityIndicator size={36} label="Loading" />
            <ScrollBar
              index={value}
              count={20}
              pageSize={5}
              onForward={() => setValue(Math.min(15, value + 1))}
              onBackward={() => setValue(Math.max(0, value - 1))}
            />
          </Section>
        </>
      )}
      {tab === 'surfaces' && (
        <>
          <Section title="Dialogs & sheets">
            <Button onPress={() => setSurface('dialog')}>Open dialog</Button>
            <Layout style={{ height: 12 }} />
            <Button variant="secondary" onPress={() => setSurface('sheet')}>
              Open bottom sheet
            </Button>
            <Layout style={{ height: 12 }} />
            <Button variant="secondary" onPress={() => setSurface('overlay')}>
              Open overlay
            </Button>
          </Section>
          <Section title="Notebook pages">
            <TabBar
              tabs={[
                { value: 'one', label: 'Page one' },
                { value: 'two', label: 'Page two' }
              ]}
              value={page}
              onValueChange={setPage}
            />
            <Notebook page={page} style={{ height: 100 }}>
              <Page name="one">
                <Text>The first page.</Text>
              </Page>
              <Page name="two">
                <Text>The second page.</Text>
              </Page>
            </Notebook>
          </Section>
          <Text variant="caption">
            Style profile: {kindle.stock.presentation}. Native contracts are simulated.
          </Text>
        </>
      )}
      <Divider />
      <Layout style={{ marginTop: 18 }}>
        <Button
          variant="secondary"
          onPress={() => scroll.current.scrollTo({ x: 0, y: 0, animated: true })}
        >
          Back to top
        </Button>
      </Layout>
      <Dialog
        visible={surface === 'dialog'}
        title="Save preferences?"
        onConfirm={() => setSurface('')}
        onCancel={() => setSurface('')}
      >
        This dialog uses the toolkit’s real composition.
      </Dialog>
      <BottomSheet
        visible={surface === 'sheet'}
        title="Reading tools"
        onDismiss={() => setSurface('')}
      >
        <Text>Your controls can live here.</Text>
        <Layout style={{ marginTop: 24 }}>
          <Button onPress={() => setSurface('')}>Done</Button>
        </Layout>
      </BottomSheet>
      {surface === 'overlay' && (
        <Overlay style={{ backgroundColor: '#fff', padding: 28 }}>
          <Text variant="title">An overlay</Text>
          <Button onPress={() => setSurface('')}>Close overlay</Button>
        </Overlay>
      )}
    </ScrollView>
  );
}
