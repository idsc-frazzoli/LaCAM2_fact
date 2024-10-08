# run this app with 'python assets/dashboard.py --map_name warehouse_small --read_from stats_large_1.json --theme dark'
# visit http://127.0.0.1:8050/ in your web browser.

import argparse
import dash_bootstrap_components as dbc
import plotly.express as px
import plotly.graph_objects as go
from dash import Dash, dcc, html

from src.data import get_data, get_additionnal_info
from src.queue import queue_graphs
from src.score import min_complexity_score, predict_score
from src.plot_layouts import beautify, beautify_bar, adjust_success_plots

"""
COLUMN NAMES
{
    "Action counts": 0,
    "Active PIBT calls": 0,
    "Active action counts": 0,
    "Algorithm": "standard",
    "Average RAM usage (Mbytes)": 0.0,
    "CPU usage (percent)": 94.0,
    "Complexity score": null,
    "Computation time (ms)": 6,
    "Loop count": 0,
    "Makespan": 55,
    "Map name": "random-32-32-10.map",
    "Maximum RAM usage (Mbytes)": 5.244,
    "Multi threading": false,
    "Number of agents": 50,
    "PIBT calls": 0,
    "Success": 1,
    "Sum of costs": 1212,
    "Sum of loss": 1212
},
"""


def show_plots(map_name: str, read_from: str=None, theme: str='dark') :
    """
    Main function to display plots using Dash.

    Args:
        map_name (str): Name of the map.
        read_from (str): Path to the data file.
        theme (str): Theme for the Dash app ('dark' or 'light').
    """

    app = Dash(__name__, external_stylesheets=[dbc.themes.BOOTSTRAP, "https://fonts.googleapis.com/css?family=Inter:300,400,500,600,700"])

    # Lightmode theme.
    if theme == 'light' :
        colors = {
            'background': '#F7F7F7',   
            'text': '#333333',
            'card': '#FFFFFF',
            'accent1': '#5A9BD5',
            'accent2': '#ED7D31',
            'line': '#b0b0b0'
        }
    # Darkmode theme.
    else :
        colors = {
            'background': '#333333',
            'text': '#F7F7F7',
            'card': '#3B3B3B',
            'accent1': '#5A9BD5',
            'accent2': '#ED7D31',
            'line': '#b0b0b0'
        }

    # For consitent color legend across the dashboard.
    color_map = {
        'FactAstar': '#19d3f3',         # Blue (light) 
        'FactBbox': '#ffa15a',          # Orange
        'FactDef': '#00d97f',           # Green
        'FactDistance': '#ab63fa',      # Purple
        'FactOrient': '#636efa',        # Blue (dark)
        'standard': '#ef553b',          # Red
        'FactPre': '#097969',           # Green (dark)
        'FactPre_distance': '#097969',  # Green (dark)
        'FactPre_bbox': '#097969',      # Green (dark)
        'FactPre_orient': '#097969',    # Green (dark)
        'FactPre_astar': '#097969'      # Green (dark)
    }


    # Gather data in specific map
    raw_data, data_success, data_success_MT, succes_rates, total_tests = get_data(map_name + '.map', read_from)
    data_std = raw_data.drop(raw_data[raw_data['Number of agents']%50 != 0].index)

    # General data split in MT / no MT
    data = raw_data.drop(raw_data[raw_data['Multi threading'] == True].index)      # without MT
    data_MT = raw_data.drop(raw_data[raw_data['Multi threading'] == False].index)     # with MT
    
    # Data for std viz split in MT / no MT. Show only every 50 agents
    data_std = data.drop(data[data['Number of agents']%5 != 0].index)     # without MT
    data_std_MT = data_MT.drop(data_MT[data_MT['Number of agents']%5 != 0].index)  # with MT
    
    # Data for success rates
    success_rate = succes_rates.drop(succes_rates[succes_rates['Multi threading'] == True].index)       # without MT
    success_rate_MT = succes_rates.drop(succes_rates[succes_rates['Multi threading'] == False].index)   # with MT
    
    # Gather additionnal data
    additionnal_info = get_additionnal_info()

    # Extra dataframe for minimum complexity score
    min_score = min_complexity_score(data.drop(data[data['Algorithm'] != "standard"].index))
    # Create the line charts
    line_CPU = px.line(data, x="Number of agents", y="CPU usage (percent)", color="Algorithm", color_discrete_map=color_map)
    line_CPU_MT = px.line(data_MT, x="Number of agents", y="CPU usage (percent)", color="Algorithm", color_discrete_map=color_map)
    line_RAM = px.line(data, x="Number of agents", y="Maximum RAM usage (Mbytes)", color="Algorithm", color_discrete_map=color_map)
    line_RAM_MT = px.line(data_MT, x="Number of agents", y="Maximum RAM usage (Mbytes)", color="Algorithm", color_discrete_map=color_map)
    line_time = px.line(data, x="Number of agents", y="Computation time (ms)", color="Algorithm", color_discrete_map=color_map)
    line_time_MT = px.line(data_MT, x="Number of agents", y="Computation time (ms)", color="Algorithm", color_discrete_map=color_map)
    line_time_std = px.scatter(data_std, x="Number of agents", y="Computation time (ms)", color="Algorithm", color_discrete_map=color_map, error_y="Computation time (ms) std")
    line_time_std_MT = px.scatter(data_std_MT, x="Number of agents", y="Computation time (ms)", color="Algorithm", color_discrete_map=color_map, error_y="Computation time (ms) std")
    line_span = px.line(data_std, x="Number of agents", y="Makespan", color="Algorithm", color_discrete_map=color_map, error_y="Makespan std")
    line_span_MT = px.scatter(data_std_MT, x="Number of agents", y="Makespan", color="Algorithm", color_discrete_map=color_map, error_y="Makespan std")
    line_costs = px.line(data_std, x="Number of agents", y="Sum of costs", color="Algorithm", color_discrete_map=color_map, error_y="Sum of costs std")
    line_score = px.line(data, x="Number of agents", y="Complexity score", color="Algorithm", color_discrete_map=color_map)

    # Add the min factorization score line :
    line_score.add_trace(go.Scatter(
        x=min_score['Number of agents'],
        y=min_score['Min complexity score'],
        mode='lines',
        name='Linear complexity',
        line=dict(color='#00d97f', dash='dash'),
    ))

    ############################# rather 'experimental' ###############################################
    # Disclaimer: unstable. This is for deeper insight in the data, not needed for overall assessment of the data.

    # Makes sense to use only when using the FactDef heuristic and actually creating partitions according to the definition.
    # Used to compare with larger number of agents to get a sense of the tendency. Use with 'stats_factdef_augmented.json ONLY.
    
    # Add the predicted factorization score line 
    # predict_score_data = data.drop(data[data['Algorithm'] != "FactDef"].index)
    # predict_score_data = predict_score(predict_score_data[['Number of agents', 'Complexity score']])

    # line_score.add_trace(go.Scatter(
    #     x=predict_score_data['Number of agents'],
    #     y=predict_score_data['Predicted Complexity score'],
    #     mode='lines',  # Use 'lines' to create a line plot
    #     line=dict(color='#00d97f', width=2),  # Line color and width
    #     name='Predicted Complexity Score'  # Name for the legend
    # ))
    # line_score.data = [line_score.data[0], line_score.data[1], line_score.data[3], line_score.data[4], line_score.data[5], line_score.data[6], line_score.data[7]]
    ################################################################################################

    # Bar charts for queue visualization (primarily for debug purposes)

    # queue_line, queue_line_MT, queue_freq, sub_ins_freq = queue_graphs(heuristic='FactDef')
    # beautify(graph=queue_freq, colors=colors, title="OPENins queue pushes", xtitle="Number of instances pushed", ytitle="Frequency", height=260, width=475)
    # beautify(graph=sub_ins_freq, colors=colors, title="Size of instances", xtitle="Number of agents in instance", ytitle="Frequency", height=260, width=475)
    ###################################################################################################

    # Create the bar charts
    bar_success_agents = px.bar(data_success, x="Number of agents", y="Success", color="Algorithm", color_discrete_map=color_map, text_auto=True, orientation='v', labels=None)
    bar_success_agents_MT = px.bar(data_success_MT, x="Number of agents", y="Success", color="Algorithm", color_discrete_map=color_map, text_auto=True, orientation='v', labels=None)
    
    bar_success_rate = px.bar(success_rate, x="Algorithm", y="Success rate", color="Algorithm", color_discrete_map=color_map, text_auto=True, orientation='v', labels=None)
    bar_success_rate_MT = px.bar(success_rate_MT, x="Algorithm", y="Success rate", color="Algorithm", color_discrete_map=color_map, text_auto=True, orientation='v', labels=None)

    # Layout updates
    beautify(graph=line_CPU, colors=colors, title="Average CPU load", xtitle="Number of agents", ytitle="Average CPU usage [%]", height=260, width=340, rangemode="tozero")
    beautify(graph=line_CPU_MT, colors=colors, title="Average CPU load (MT)", xtitle="Number of agents", ytitle="Average CPU usage [%]", height=260, width=340, rangemode="tozero")
    beautify(graph=line_RAM, colors=colors, title="Max. RAM load", xtitle="Number of agents", ytitle="Max. RAM usage [Mb]", height=260, width=340, rangemode="tozero")
    beautify(graph=line_RAM_MT, colors=colors, title="Max. RAM load (MT)", xtitle="Number of agents", ytitle="Max. RAM usage [Mb]", height=260, width=340, rangemode="tozero")
    beautify(graph=line_time, colors=colors, title="Computation time [ms]", xtitle="Number of agents", height=260, width=475, rangemode="tozero")
    beautify(graph=line_time_MT, colors=colors, title="Computation time [ms] (MT)", xtitle="Number of agents",  height=260, width=475, rangemode="tozero")
    beautify(graph=line_time_std, colors=colors, title="Computation time [ms]", xtitle="Number of agents", height=260, width=475, rangemode="tozero")
    beautify(graph=line_time_std_MT, colors=colors, title="Computation time [ms] (MT)", xtitle="Number of agents",  height=260, width=475, rangemode="tozero")
    beautify(graph=line_span, colors=colors, title="Makespan", xtitle="Number of agents",  height=280, width=475)
    beautify(graph=line_costs, colors=colors, title="Sum of costs", xtitle="Number of agents",  height=280, width=475, legend=True)
    beautify(graph=line_score, colors=colors, title="Complexity score", xtitle="Number of agents", ytitle="log(score)", height=280, width=475, rangemode="tozero", legend=True)
    
    beautify_bar(graph=bar_success_agents, colors=colors, title="Success rate [%]", xtitle="Number of agents",  height=260, width=475, legend=True)
    beautify_bar(graph=bar_success_agents_MT, colors=colors, title="Success rate [%] (MT)", xtitle="Number of agents",  height=260, width=475, legend=True)
    beautify_bar(graph=bar_success_rate, colors=colors, title="Overall success rate [%]",  height=212, width=295, legend=True)
    beautify_bar(graph=bar_success_rate_MT, colors=colors, title="Success rate [%] (MT)",  height=212, width=295)

    # Manage the x_axis of the success plots
    adjust_success_plots(bar_data=data_success['Number of agents'], colors=colors, bar_success_agents=bar_success_agents, bar_success_agents_MT=bar_success_agents_MT)
    

    print("\nDashboard updated")


    # Layout of the Dashboard
    app.layout = html.Div(style={'backgroundColor': colors['background'], 'fontFamily': 'Inter, sans-serif'}, children=[
        
        # Title Row
        dbc.Row(
            dbc.Col(
                html.Div(html.P(["PERFORMANCE OVERVIEW"])), 
                style={'textAlign': 'center', 'paddingTop': '25px', 'color': colors['text'], 'fontSize': 25, 'fontWeight': 'bold', 'height': '90px'}
            ),
        ),

        # First Row
        dbc.Row(
            [
                # Left column: Map and algorithm details
                dbc.Col(
                    html.Div([
                        html.H5("General Information", style={'color': colors['text'], 'fontWeight': 'bold'}),
                        html.Div([
                            html.P(html.Strong("Map tested"), style={'fontSize': '15px', 'color': colors['text'], 'display': 'inline-block', 'width': '170px'}),
                            html.P(f"   {map_name}.map", style={'fontSize': '15px', 'color': colors['text'], 'display': 'inline-block'})
                        ], style={'height': '30px'}),
                        html.Div([
                            html.P(html.Strong("Agents range"), style={'fontSize': '15px', 'color': colors['text'], 'display': 'inline-block', 'width': '170px'}),
                            html.P(f"   {additionnal_info['Agent range'][0]} to {additionnal_info['Agent range'][1]}", style={'fontSize': '15px', 'color': colors['text'], 'display': 'inline-block'})
                        ], style={'height': '30px'}),
                        html.Div([
                            html.P(html.Strong("Tests per algorithm"), style={'fontSize': '15px', 'color': colors['text'], 'display': 'inline-block', 'width': '170px'}),
                            html.P(f"   {total_tests}", style={'fontSize': '15px', 'color': colors['text'], 'display': 'inline-block'})
                        ], style={'height': '30px'}),
                    ], style={'backgroundColor': colors['card'], 'marginLeft': '30px', 'padding': '15px', 'width': '475px'}),
                    width=3,
                    style={'backgroundColor': colors['background']}
                ),

                # Middle column: Hardware information
                dbc.Col(
                    html.Div([
                        html.H5("Hardware Information", style={'color': colors['text'], 'fontWeight': 'bold'}),
                        html.Div([
                            html.P(html.Strong("CPU model"), style={'fontSize': '15px', 'color': colors['text'], 'display': 'inline-block', 'width': '100px'}),
                            html.P(f"   {additionnal_info['CPU model']}", style={'fontSize': '15px', 'color': colors['text'], 'display': 'inline-block'})
                        ], style={'height': '30px'}),
                        html.Div([
                            html.P(html.Strong("CPU cores"), style={'fontSize': '15px', 'color': colors['text'], 'display': 'inline-block', 'width': '100px'}),
                            html.P(f"   {additionnal_info['CPU cores']}", style={'fontSize': '15px', 'color': colors['text'], 'display': 'inline-block'})
                        ], style={'height': '30px'}),
                        html.Div([
                            html.P(html.Strong("RAM size"), style={'fontSize': '15px', 'color': colors['text'], 'display': 'inline-block', 'width': '100px'}),
                            html.P(f"   {additionnal_info['RAM size']}", style={'fontSize': '15px', 'color': colors['text'], 'display': 'inline-block'})
                        ], style={'height': '30px'}),
                        html.Div([
                            html.P(html.Strong("OS name"), style={'fontSize': '15px', 'color': colors['text'], 'display': 'inline-block', 'width': '100px'}),
                            html.P(f"   {additionnal_info['OS name']}", style={'fontSize': '15px', 'color': colors['text'], 'display': 'inline-block'})
                        ], style={'height': '30px'}),
                        html.Div([
                            html.P(html.Strong("OS version"), style={'fontSize': '15px', 'color': colors['text'], 'display': 'inline-block', 'width': '100px'}),
                            html.P(f"   {additionnal_info['OS version']}", style={'fontSize': '15px', 'color': colors['text'], 'display': 'inline-block'})
                        ], style={'height': '30px'}),
                    ], style={'backgroundColor': colors['card'], 'marginLeft': '15px', 'padding': '15px', 'width': '485px'}),
                    width=4,
                    style={'backgroundColor': colors['background']}
                ),

                # Right column: Success rate bar chart and total tests executed
                dbc.Col(
                    html.Div(dcc.Graph(id='Graph0', figure=bar_success_rate)),
                    width=2,
                    style={'backgroundColor': colors['background']}
                ),
                dbc.Col(
                    html.Div(dcc.Graph(id='Graph01', figure=bar_success_rate_MT)),
                    width=2,
                    style={'backgroundColor': colors['background'], 'marginLeft': '60px'}
                ),
            ],
            style={'marginBottom': '30px'}
        ),

        # Second row, sequential solving data
        dbc.Row(
            [
                dbc.Col(dcc.Graph(id='graph1',figure=line_time, style={'marginLeft': '30px'}), width=4, style={'textAlign': 'center', 'borderRadius': '10px'}),
                dbc.Col(dcc.Graph(id='graph2',figure=line_time_std, style={'marginLeft': '15px'}), width=4, style={'textAlign': 'center'}),
                dbc.Col(dcc.Graph(id='graph3',figure=bar_success_agents), width=4, style={'textAlign': 'center'})
            ],
            style={'marginBottom': '30px'}
        ),

        # Third row, multi processing data
        dbc.Row(
            [
                dbc.Col(dcc.Graph(id='graph4',figure=line_time_MT, style={'marginLeft': '30px'}), width=4, style={'textAlign': 'center'}),
                dbc.Col(dcc.Graph(id='graph5',figure=line_time_std_MT, style={'marginLeft': '15px'}), width=4, style={'textAlign': 'center'}),
                dbc.Col(dcc.Graph(id='graph6',figure=bar_success_agents_MT), width=4, style={'textAlign': 'center'})
            ],
            style={'marginBottom': '30px'}
        ),

        # Fourth row, complexity score, solution quality
        dbc.Row(
            [
                dbc.Col(dcc.Graph(id='graph7',figure=line_score, style={'marginLeft': '30px'}), width=4, style={'textAlign': 'center'}),
                dbc.Col(dcc.Graph(id='graph8',figure=line_span, style={'marginLeft': '15px'}), width=4, style={'textAlign': 'center'}),
                dbc.Col(dcc.Graph(id='graph9',figure=line_costs, style={'marginLeft': '15px'}), width=4, style={'textAlign': 'center'})
            ],
            style={'marginBottom': '30px'}
        ),
        
        # Fifth row, hardware data
        dbc.Row(
            [
                #dbc.Col(width=4, style={'textAlign': 'center'}),
                dbc.Col(dcc.Graph(id='graph10',figure=line_RAM, style={'marginLeft': '30px'}), width=3, style={'textAlign': 'center'}),
                dbc.Col(dcc.Graph(id='graph11',figure=line_RAM_MT, style={'marginLeft': '17px'}), width=3, style={'textAlign': 'center'}),
                dbc.Col(dcc.Graph(id='graph12',figure=line_CPU, style={'marginLeft': '13'}), width=3, style={'textAlign': 'center'}),
                dbc.Col(dcc.Graph(id='graph13',figure=line_CPU_MT, style={'marginLeft': '10'}), width=3, style={'textAlign': 'center'})
            ],
            style={'marginBottom': '30px'}
        ),

        # Sixth row. Used for visualizing the queues. Reads data from the last found partitions. Not very comprehensive, was mainly used for debugging purposes
        dbc.Row(
            [
                # dbc.Col(dcc.Graph(id='graph14',figure=queue_freq, style={'marginLeft': '30px'}), width=4, style={'textAlign': 'center'}),
                # dbc.Col(dcc.Graph(id='graph15',figure=sub_ins_freq, style={'marginLeft': '15px'}), width=4, style={'textAlign': 'center'}),
            ],
            style={'marginBottom': '30px'}
        ),

        # Final padding
        dbc.Row(dbc.Col(html.Div("")), style={'height' : '200px'}),

    ])

    if __name__ == '__main__':
        app.run(debug=True)



def main():

    """
    Parse command-line arguments and launch the Dash application with the specified parameters.

    Command-line arguments:
        --map_name: The name of the map to display. This argument is required.
        --read_from: The file to read data from. This argument is optional.
        --theme: The theme of the application, which can be 'dark' or 'light'. Defaults to 'dark'.
    """
    parser = argparse.ArgumentParser(description='Launch the Dash application with specified parameters.')
    parser.add_argument('--map_name', type=str, required=True, help='Name of the map to display')
    parser.add_argument('--read_from', type=str, help='File to read data from')
    parser.add_argument('--theme', type=str, choices=['dark', 'light'], default='dark', help='Theme of the application')

    args = parser.parse_args()
    
    show_plots(map_name=args.map_name, read_from=args.read_from, theme=args.theme)

if __name__ == '__main__':
    main()
