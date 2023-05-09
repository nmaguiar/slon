# SLON

## Introduction

SLON (Single Line Object Notation) is a small modification to JSON (JavaScript Object Notation) to make it easier, and less painfull, for humans to quickly read data on a single line (e.g. on a notification, on a small text line, etc...). In a sense it's like YAML but in a single line.

Example of a weather notification using SLON:

````
The current conditions are (condition: Moderate Rain, temp: 12.2, feelsLike: 14, sunLight: true, date: 2023-02-05/12:34:45.678)
````

The same using JSON:

````
The current conditions are {"condition":"Moderate Rain","temp":12.2,"feelsLike":14,"sunLight":true,"date":"2023-02-05/12:34:45.678"}
````

## Changes from JSON

The following table presents the changes from JSON:

| Type | JSON delimiter | SLON delimiter |
|------|----------------|----------------|
| Map begin | { | ( |
| Map end | } | ) |
| Array values delimiter | , | \| |

Additionally:
* dates are simplified from ````1234-12-24T12:34:56.123Z```` to ````1234-12-24/12:34:56.123````
* map keys don't need to be quoted unless they contain ":" or quotes
* string values don't need to be quoted unless they contain "," or quotes 

## Example with all types

In SLON:

````
(number: 123.12, boolean: true, null_value: null, array: [1 | 'item' | 3], map: (x: -1, y: 1), date: 1235-01-17/12:34:56.123)
````

In JSON:

````
{"number":123.12,"boolean":true,"null_value":null,"array":[1,"item",3],"map":{"x":-1,"y":1},"date":"1235-01-24T12:34:56.123Z"}
````
