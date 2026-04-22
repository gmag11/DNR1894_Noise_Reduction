## Context

El LM1894 implementa un sistema DNR no complementario para reproducción estéreo. Su núcleo audible es un par de filtros paso bajo con pendiente de 6 dB/octava cuyo ancho de banda varía aproximadamente entre 1 kHz y 35 kHz. Ambos canales comparten una única ruta de control para evitar desplazamientos de la imagen estéreo. La ruta de control suma L+R, aplica ponderación en frecuencia, detecta picos y gobierna la apertura del filtro con un ataque típico del orden de 500 us y un decay del orden de 60 ms.

La nota de aplicación AN-0390 añade contexto importante para producto: calibración por umbral, ubicación del procesado antes de tono/volumen, perfiles prácticos para cassette, FM y TV, y el hecho de que la reducción subjetiva del hiss depende del espectro y del nivel de ruido de la fuente.

## Goals / Non-Goals

**Goals:**
- Modelar el comportamiento audible principal del LM1894 en un plugin VST estéreo de tiempo real.
- Preparar el producto para distribución multiplataforma sobre Windows y macOS sin duplicar la implementación del DSP.
- Empezar con una entrega mínima en Windows VST3 sin cerrar el camino a AU, standalone o Linux VST3/standalone.
- Mantener una única ruta de control enlazada para preservar la imagen estéreo.
- Permitir una arquitectura opcional multietapa para obtener mayor reducción y pendientes más pronunciadas.
- Exponer controles que permitan calibrar sensibilidad, rango de ancho de banda y dinámica temporal.
- Permitir validación objetiva frente a objetivos extraídos del datasheet y la nota de aplicación.

**Non-Goals:**
- Emular el silicio a nivel transistor o reproducir exactamente tensiones internas de cada pin.
- Simular el resto de la cadena analógica externa completa de cassette, FM o TV.
- Reproducir intencionadamente limitaciones de alimentación, PSRR o distorsión no relevantes para el resultado audible principal.
- Soportar desde la primera iteración todos los formatos de plugin posibles; la prioridad es VST3 y, si conviene al producto, AU y standalone usando la misma base.
- Introducir dependencias específicas de plataforma dentro del núcleo DSP o del modelo de parámetros.

## Decisions

### 1. Modelado funcional, no eléctrico

La primera versión se especifica como una simulación funcional del comportamiento audible del LM1894, no como una emulación circuito a circuito. Esto mantiene el alcance controlado y alinea la implementación con lo que realmente importa en un plugin: la respuesta espectral dinámica, la preservación de imagen estéreo y la sensación de reducción de hiss.

### 2. JUCE como framework base de producto

JUCE es una elección técnicamente correcta para este proyecto porque separa bien las preocupaciones de producto y de DSP: permite construir VST3, AU y aplicación standalone con una misma base de código, resuelve gran parte de la integración con hosts y automatización, y deja el modelo LM1894 encapsulado en una capa DSP propia.

La decisión no implica acoplar el algoritmo al framework. El núcleo DSP debe quedar aislado en una capa portable y testeable, con JUCE encargado de:
- ciclo de vida del plugin;
- parámetros, presets y automatización;
- editor y metering;
- targets y build multiplataforma.

```text
    +---------------------------+
    |        JUCE Shell         |
    | VST3 / AU / Standalone    |
    +------------+--------------+
                     |
        parameters|audio buffers
                     v
    +---------------------------+
    |     LM1894 DSP Core       |
    | sidechain + filters + N   |
    | cascaded stages           |
    +---------------------------+
```

Esto reduce riesgo de reescritura posterior si el plugin termina necesitando más de un formato de entrega.

### 3. Estrategia de targets por fases

La estrategia recomendada es:

1. V1 inicial: Windows + VST3.
2. V1.x: añadir standalone sobre la misma base JUCE para depuración y validación fuera del host.
3. V2: añadir macOS + AU.
4. V2.x: añadir Linux con VST3 y standalone si la validación en host lo justifica.

Esto mantiene el arranque pequeño sin tomar decisiones que luego obliguen a rehacer la estructura. AU sólo tiene sentido en macOS, así que la ruta natural para Linux debe ser VST3 y standalone, no AU.

La clave no es activar todos los targets desde el primer día, sino que el proyecto quede organizado para que cada nuevo formato sea una integración adicional, no una reescritura.

### 4. Arquitectura del proyecto

La arquitectura propuesta separa producto, parámetros y DSP en capas explícitas:

```text
repo/
    plugin/
        juce/
            CMakeLists.txt
            Source/
                PluginProcessor.*
                PluginEditor.*
                StandaloneMain.*
    src/
        dsp/
            Lm1894Model.h
            Lm1894Model.cpp
            SidechainDetector.h
            SidechainDetector.cpp
            VariableLpStage.h
            VariableLpStage.cpp
            CascadedFilterBank.h
            CascadedFilterBank.cpp
        params/
            ParameterIds.h
            ParameterLayout.h
            ParameterLayout.cpp
            PresetProfiles.h
            PresetProfiles.cpp
        app/
            PluginState.h
            PluginState.cpp
            MeterState.h
            MeterState.cpp
    tests/
        dsp/
        integration/
    docs/
```

Responsabilidades por capa:
- `src/dsp`: algoritmo puro, sin clases UI ni tipos dependientes del host.
- `src/params`: definición única de parámetros, rangos, escalados, defaults y perfiles.
- `src/app`: estado compartido entre shell de plugin, DSP y metering.
- `plugin/juce`: pegamento con JUCE, formatos de plugin, editor y lifecycle del host.
- `tests`: validación del modelo y pruebas de integración del plugin.

### 5. Flujo de audio y control

```text
Host/DAW
     |
     v
JUCE AudioProcessor
     |
     +--> Parameter snapshot -----------------------------+
     |                                                    |
     v                                                    v
Input buffer --> LM1894 Model --> Output buffer --> MeterState --> Editor
                                    |
                                    +--> Stereo sum -> weighting -> detector -> cutoff target
                                                                            |
                                                                            +--> stage compensation -> N cascaded stages
```

La regla importante es que el `AudioProcessor` no contenga el algoritmo en línea. Debe actuar como adaptador entre buffers del host, snapshots atómicos de parámetros y el núcleo `Lm1894Model`.

### 6. Modelo de parámetros

Los parámetros deben definirse una sola vez en `src/params` y consumirse desde JUCE sin duplicar semántica. El conjunto mínimo queda así:
- `Sensitivity`
- `MinBandwidthHz`
- `MaxBandwidthHz`
- `AttackMs`
- `ReleaseMs`
- `StageCount`
- `SourceProfile`
- `Bypass`
- `OutputTrimDb`
- `Mix` si finalmente se considera útil

Cada parámetro debe tener:
- identificador estable;
- rango normalizado para automatización;
- conversión a unidades físicas internas;
- valor por defecto;
- texto de display desacoplado del DSP.

Para la V1, el contrato recomendado queda fijado así:

| Parameter ID | Tipo | Rango | Default | Notas |
|---|---|---:|---:|---|
| `sensitivityDb` | float | -18 dB a +18 dB | 0 dB | Offset respecto al umbral nominal del perfil |
| `minBandwidthHz` | float | 800 Hz a 4000 Hz | 1000 Hz | Límite inferior del cutoff global |
| `maxBandwidthHz` | float | 8000 Hz a 40000 Hz | 35000 Hz | Límite superior del cutoff global |
| `attackMs` | float | 0.05 ms a 5.0 ms | 0.5 ms | Objetivo LM1894-like de apertura rápida |
| `releaseMs` | float | 10 ms a 250 ms | 60 ms | Cierre más lento para preservar ambiente |
| `stageCount` | int | 1 a 4 | 1 | 1 preserva el comportamiento base; 2..4 añaden pendiente y reducción |
| `sourceProfile` | enum | `generic`, `tape`, `fm`, `tv` | `tape` | Determina ponderación y posibles muescas del sidechain |
| `bypass` | bool | off/on | off | Bypass duro del procesado |
| `outputTrimDb` | float | -12 dB a +12 dB | 0 dB | Compensa nivel percibido entre ajustes |

Decisiones adicionales para V1:
- `Mix` queda fuera de la V1 porque no forma parte del comportamiento original ni es necesario para validar el modelo.
- `stageCount` debe exponerse como entero discreto, no como continuo automatizado.
- `minBandwidthHz` nunca podrá superar `maxBandwidthHz`; si el usuario cruza ambos valores, el layout debe imponer saturación o reordenación determinista.
- `sourceProfile` debe tener semántica fija: `generic` sin notch adicional, `tape` calibración base, `fm` con supresión del piloto de 19 kHz y `tv` con supresión en torno a 15.734 kHz.

Representación interna recomendada:

```text
Normalized host value
    |
    v
ParameterLayout conversion
    |
    v
Lm1894Parameters snapshot
    |
    v
Lm1894Model::setParameters(...)
```

Esto evita que el DSP dependa de rangos normalizados del host o de clases específicas de JUCE.

### 7. Estado y presets

`PluginState` debe encapsular:
- snapshot de parámetros activos;
- serialización de estado para el host;
- perfil de fuente seleccionado;
- versión interna de preset o schema de estado.

Esto evita que el serializado dependa de detalles del editor o de objetos concretos de JUCE difíciles de migrar.

La API mínima recomendada para la capa de aplicación queda así:

```cpp
enum class SourceProfile
{
    generic,
    tape,
    fm,
    tv,
};

struct Lm1894Parameters
{
    float sensitivityDb;
    float minBandwidthHz;
    float maxBandwidthHz;
    float attackMs;
    float releaseMs;
    int stageCount;
    SourceProfile sourceProfile;
    bool bypass;
    float outputTrimDb;
};

struct ProcessSpec
{
    double sampleRate;
    uint32_t maximumBlockSize;
    uint32_t numChannels;
};

struct StereoBufferView
{
    float* left;
    float* right;
    uint32_t numSamples;
};

struct Lm1894MeterValues
{
    float normalizedBandwidthOpen;
    float detectorActivity;
    float estimatedReductionDb;
    int activeStageCount;
    bool bypass;
};

class Lm1894Model
{
public:
    void prepare(const ProcessSpec& spec);
    void reset();
    void setParameters(const Lm1894Parameters& parameters);
    void process(StereoBufferView buffer);
    Lm1894MeterValues getMeterValues() const;
};
```

Criterios de esta API:
- sin tipos JUCE en la firma pública del DSP;
- una sola llamada `setParameters` por snapshot para evitar incoherencias entre parámetros dependientes;
- `prepare` y `reset` separados para distinguir cambios de formato de reinicios de estado;
- `getMeterValues` como lectura ligera apta para publicar en `MeterState`.

Subcomponentes internos recomendados, no expuestos al host:
- `SidechainDetector` para suma estéreo, ponderación, notch y envolvente;
- `CascadedFilterBank` para 1..N etapas enlazadas;
- `VariableLpStage` como unidad de filtrado por etapa;
- `CutoffMapper` o lógica equivalente para convertir detector + límites + número de etapas en cutoff por etapa.

### 8. Metros y editor

El editor JUCE debe leer un `MeterState` thread-safe alimentado por el hilo de audio con datos mínimos agregados, por ejemplo:
- apertura de ancho de banda actual;
- actividad del detector;
- número de etapas activas;
- estado de bypass.

No debe exponer referencias directas al estado interno del filtro ni forzar locks en tiempo real.

### 9. Build y portabilidad

El build debería apoyarse en CMake con JUCE para evitar depender del Projucer como fuente única de verdad. La configuración inicial recomendada es:
- target `Lm1894Vst3` en Windows;
- opción de compilar `Standalone` en el mismo proyecto;
- flags o presets para habilitar macOS AU y Linux VST3/standalone más adelante.

Esto te deja una ruta de migración bastante limpia:
- el DSP y parámetros no cambian;
- cambia sólo el packaging de salida y las tareas de CI/firmado por plataforma.

### 10. Arquitectura interna enlazada en estéreo

El plugin usará dos rutas de audio equivalentes y una sola ruta de control compartida.

```text
        Left In --------------------> Variable LPF -----------------> Left Out
            |                               ^
            |                               |
            +----\                     control signal
                  > Sum -> Weight -> Peak/Envelope -> Bandwidth map
            +----/                     control signal
            |
        Right In -------------------> Variable LPF -----------------> Right Out
```

Esta decisión refleja el comportamiento del integrado y evita que un evento brillante en un solo canal estreche o abra sólo un lado del estéreo.

### 11. Un filtro principal simple y continuamente variable por etapa

La pendiente objetivo base en cada etapa será equivalente a un paso bajo de primer orden por canal. La simulación debe priorizar continuidad temporal del cutoff y ausencia de zipper noise sobre exactitud topológica de la implementación analógica. La forma concreta del filtro digital queda abierta siempre que cumpla los requisitos de respuesta y estabilidad definidos en la spec.

### 12. Modo multietapa con sidechain global compartido

El plugin podrá encadenar varias etapas idénticas del mismo proceso en serie para aumentar la pendiente efectiva del filtrado y la reducción de hiss. Todas las etapas deberán compartir el mismo estado de control para conservar un comportamiento coherente y evitar aperturas divergentes entre etapas.

```text
        Left In ---> Stage 1 LPF ---> Stage 2 LPF ---> ... ---> Stage N LPF ---> Left Out
            |             ^               ^                           ^
            |             |               |                           |
            +------\      |               |                           |
                   > Sum -> Weight -> Peak/Envelope -> Bandwidth map --+
            +------/      |               |                           |
            |             |               |                           |
        Right In --> Stage 1 LPF ---> Stage 2 LPF ---> ... ---> Stage N LPF --> Right Out
```

La referencia analógica de AN-0390 documenta explícitamente el caso de dos etapas equivalentes para lograr una pendiente de 12 dB/oct y hasta unos 18 dB de reducción. En el plugin, la arquitectura deberá generalizar esa idea a varias etapas sin cambiar el principio del proceso.

### 13. Compensación del cutoff al variar el número de etapas

Al cascadar etapas, el punto de -3 dB combinado cae por debajo del cutoff individual de cada filtro. Para que el control de ancho de banda siga significando aproximadamente lo mismo al pasar de 1 a N etapas, el diseño debe compensar el cutoff por etapa a partir del objetivo global.

Para un banco de N etapas de primer orden idénticas, la relación objetivo es:

$$f_{c,global} = f_{c,stage} \sqrt{10^{0.3/N} - 1}$$

Por tanto, el diseño interno deberá calcular un cutoff por etapa a partir del cutoff global deseado, en lugar de reutilizar sin más el mismo cutoff por etapa y aceptar un estrechamiento accidental del rango útil.

### 14. Curva de mapeo entre detector y cutoff global

La revisión del datasheet y de las gráficas relevantes obliga a fijar una decisión más precisa: el mapeo entre actividad del detector y cutoff global no debe modelarse como una interpolación lineal simple.

Las referencias más útiles son:
- Figure 8 del datasheet: `Main Signal Path Bandwidth vs Voltage Control`, que muestra una relación fuertemente no lineal entre señal de control y ancho de banda.
- Figure 12 del datasheet: `Output vs Frequency`, que confirma que el sistema es lineal en amplitud de salida pero no en ancho de banda efectivo para tonos estacionarios.
- Figure 13 del datasheet: `-3 dB Bandwidth vs Frequency and Control Signal`, que muestra que distintas frecuencias en la ruta de control generan diferentes familias de roll-off.
- Figure 7 del datasheet: `Gain of Control Path vs Frequency`, que deja claro que la ruta de control pondera con mucha más fuerza la energía entre aproximadamente 2 kHz y 10 kHz que el contenido por debajo de 1 kHz.

La consecuencia práctica es que la simulación V1 debe usar tres dominios distintos:

```text
Audio estéreo
    -> señal ponderada de sidechain
    -> envolvente / detector equivalente
    -> curva no lineal en log-frecuencia
    -> cutoff global objetivo
```

#### 14.1 Ponderación del sidechain

La ruta de control digital debe aproximar estas propiedades observadas en las gráficas:
- sensibilidad muy reducida por debajo de 200 Hz a 500 Hz;
- crecimiento fuerte entre aproximadamente 500 Hz y 4 kHz;
- máxima sensibilidad útil en la zona de 4 kHz a 10 kHz;
- notch opcional en 19 kHz para perfil FM;
- notch opcional alrededor de 15.734 kHz para perfil TV.

No hace falta reproducir exactamente la topología RC del chip, pero sí la intención espectral: contenido grave no debe abrir apreciablemente el ancho de banda, mientras que hiss y contenido brillante sí deben hacerlo.

#### 14.2 Variable interna de detector equivalente

Para alinear el diseño con la documentación del LM1894, el modelo interno usará una variable de detector equivalente `v_eq` normalizada al rango eléctrico documentado del chip.

- mínimo de referencia: `v_eq_min = 1.1 V`
- máximo de referencia: `v_eq_max = 3.8 V`

La sensibilidad y el perfil de fuente desplazan horizontalmente la curva al convertir la envolvente del sidechain a `v_eq`.

Esto permite dos cosas:
- razonar sobre calibración usando el lenguaje del datasheet y AN-0390;
- desacoplar la medición interna del detector de la implementación concreta del follower digital.

#### 14.3 Mapeo recomendado de `v_eq` a cutoff global

La V1 debe usar una curva monotónica por puntos interpolada en log-frecuencia, no una recta. La siguiente tabla resume el comportamiento objetivo para el preset base `tape` con rango por defecto cercano a 1 kHz-35 kHz:

| `v_eq` equivalente | Cutoff global objetivo |
|---:|---:|
| 1.10 V | 1.0 kHz |
| 1.25 V | 1.3 kHz |
| 1.35 V | 1.8 kHz |
| 1.50 V | 2.5 kHz |
| 1.75 V | 4.0 kHz |
| 2.05 V | 7.0 kHz |
| 2.40 V | 12.0 kHz |
| 2.80 V | 20.0 kHz |
| 3.20 V | 28.0 kHz |
| 3.80 V | 35.0 kHz |

La forma exacta de interpolación recomendada es:
- interpolación monotónica suave entre puntos;
- dominio de interpolación en `log10(f)` y no en Hz lineales;
- clamp duro a los límites mínimo y máximo de ancho de banda configurados por el usuario.

Esta tabla intenta conciliar tres hechos documentados:
- el mínimo absoluto puede acercarse a 1 kHz;
- la nota del datasheet indica que en operación real el sistema suele trabajar con un mínimo nominal más próximo a 2 kHz para lograr alrededor de 10 dB de reducción subjetivamente útil;
- la gráfica Figure 8 muestra una apertura progresivamente más rápida al acercarse a la zona alta del ancho de banda.

#### 14.4 Regla de escalado para otros rangos

Cuando el usuario cambie `minBandwidthHz` o `maxBandwidthHz`, la curva base no debe rehacerse en Hz absolutos. Debe conservar su forma relativa en log-frecuencia.

La regla recomendada es:

$$
\log_{10}(f_c(u)) = \log_{10}(f_{min}) + s(u)\,\left[\log_{10}(f_{max}) - \log_{10}(f_{min})\right]
$$

donde `s(u)` es una curva de forma fija derivada de la tabla anterior y `u` es la versión normalizada de `v_eq` en el rango $[0,1]$.

Esto hace que cambiar el rango configurable no destruya la personalidad del LM1894.

#### 14.5 Regla de calibración por perfil

Los perfiles no deben cambiar la tabla de mapeo principal salvo que sea estrictamente necesario. La V1 debe modificar primero:
- la ponderación del sidechain;
- el offset de sensibilidad;
- los notches opcionales.

El objetivo es que `sourceProfile` cambie la forma en que el detector llega a `v_eq`, no la personalidad básica del filtro una vez que el detector ya decidió abrirlo.

#### 14.6 Consecuencia para defaults

Aunque el mínimo absoluto por defecto se mantenga en 1 kHz para respetar el chip, el preset `tape` debe calibrarse para que el ruido de cinta en reposo tienda a abrir ligeramente el sistema hacia una zona nominal cercana a 1.8 kHz-2.2 kHz, no a quedarse pegado al mínimo absoluto. Esto está más alineado con AN-0390 y con la nota del datasheet sobre la reducción típica de alrededor de 10 dB.

### 15. Parámetros físicos internos separados de controles de usuario

La implementación puede mantener parámetros internos expresados como sensibilidad, frecuencia mínima, frecuencia máxima, ataque y release, mientras que la UI podrá agruparlos en controles más musicales o más cercanos al datasheet. Esto permite una experiencia de usuario razonable sin perder trazabilidad con el comportamiento del LM1894.

El número de etapas formará parte de esos parámetros expuestos o de un modo avanzado claramente visible, ya que modifica de forma audible la pendiente y la reducción alcanzable.

### 16. Perfiles de fuente como presets controlados

La spec incluirá perfiles orientados a fuentes reales porque AN-0390 demuestra que la calibración depende del ruido de origen. En vez de obligar a recrear toda la electrónica de cassette, FM o TV, esos perfiles actuarán como presets reproducibles sobre la ruta de control: sensibilidad, ponderación y, cuando aplique, muesca en sidechain para 19 kHz o 15.734 kHz.

### 17. Estrategia multiplataforma

La primera planificación de producto debe asumir:
- Windows como target principal de desarrollo inicial;
- macOS como segundo target soportado por la misma base JUCE;
- formato VST3 como mínimo de entrega;
- AU como salida específica de macOS una vez estabilizada la base;
- Linux orientado a VST3 y standalone, no a AU.

El punto importante es no mezclar lógica de UI o clases de JUCE dentro del núcleo DSP. Si el DSP permanece agnóstico al framework, validar comportamiento y mantener el proyecto será bastante más simple.

### 18. Validación basada en comportamiento

La aceptación no se basará sólo en escucha subjetiva. Se exigirán pruebas con bursts, ruido y barridos para comprobar que el tiempo de apertura/cierre, el enlace estéreo y el rango de cutoff se comportan dentro de tolerancias razonables respecto al LM1894 documentado.

Además, la validación de la V1 debe incluir comprobaciones explícitas del mapeo:
- que el sidechain apenas abra el sistema con energía concentrada por debajo de 500 Hz;
- que el mismo nivel efectivo en la zona de 5 kHz a 8 kHz abra notablemente más el cutoff;
- que el preset `tape` con ruido de referencia mantenga una apertura nominal cercana a 2 kHz;
- que la curva `v_eq -> cutoff` preserve su forma al escalar `minBandwidthHz` y `maxBandwidthHz`.

### 19. Plan técnico para una V1 ampliable

El trabajo posterior debería seguir este desglose:

1. Definir una variable global de cutoff objetivo a partir del sidechain compartido.
2. Derivar el cutoff por etapa con compensación por número de etapas.
3. Aplicar el mismo cutoff compensado a N etapas idénticas en ambos canales.
4. Encapsular el DSP en una capa independiente de JUCE con API clara para proceso, reset y actualización de parámetros.
5. Definir `ParameterLayout` y `PluginState` como contrato estable entre host, DSP y editor.
6. Montar un plugin JUCE con VST3 para Windows como target mínimo y dejar preparada la salida standalone.
7. Exponer `Stage Count` y validar que el cambio entre valores mantiene estabilidad, nivel razonable y ausencia de clicks.
8. Añadir pruebas comparativas entre 1, 2 y N etapas para cuantificar la reducción extra y el posible oscurecimiento del material.
9. Cuando la base esté estable, activar AU en macOS y VST3/standalone en Linux sin tocar el núcleo DSP.

## Risks / Trade-offs

- La relación exacta entre señal, ruido y cutoff del LM1894 es no lineal; una simulación digital demasiado simplificada puede sonar bien pero alejarse de la dinámica real del chip.
- Más etapas aumentan la reducción, pero también elevan el riesgo de apagar demasiado el contenido brillante o de producir una sensación menos natural en material rico en agudos.
- Si cada etapa tuviera control independiente, el comportamiento sería más complejo pero menos fiel al principio de control común; por eso se prefiere un sidechain global.
- JUCE acelera mucho el producto multiplataforma, pero añade una capa de framework que conviene aislar del DSP para no dificultar pruebas ni portabilidad futura.
- Si el layout de parámetros o la serialización se mezclan demasiado con JUCE desde el inicio, la migración entre formatos será más frágil de lo necesario.
- Los perfiles FM y TV pertenecen más al contexto de aplicación que al núcleo del IC; incluirlos mejora utilidad práctica, pero introduce alcance adicional.
- Si la automatización o el cambio de cutoff no se suavizan correctamente, el plugin puede introducir artefactos que el hardware original no produce de manera perceptible.
- La reducción subjetiva de ruido depende del material; por eso la spec debe definir métricas objetivas y no sólo promesas auditivas amplias.