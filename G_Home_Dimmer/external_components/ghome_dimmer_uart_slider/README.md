# GHome Dimmer UART Slider

ESPHome external component for controlling GHome dimmer devices via UART.

## Usage

Add this to your ESPHome configuration:

```yaml
external_components:
  - source:
      type: local
      path: components/ghome_dimmer_uart_slider

ghome_dimmer_uart_slider:
  id: slider
  uart_id: uart_bus
  switch_light: switch_light  # Optional: light to control
```

## Configuration

- **id** (Required, ID): The ID for this component
- **uart_id** (Required, ID): The UART bus to use for communication
- **switch_light** (Optional, ID): The light state to sync with the dimmer

## How It Works

This component reads UART data from GHome dimmer touch sliders and updates the connected light state accordingly. It also syncs the light state to the dimmer on boot.