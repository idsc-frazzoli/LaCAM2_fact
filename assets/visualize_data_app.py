# Run this app with `python app.py` and
# visit http://127.0.0.1:8050/ in your web browser.



from dash import Dash, dcc, html
from analyze_data import get_data
import dash_bootstrap_components as dbc
import plotly.express as px

"""
COLUMN NAMES
{
    "Number of agents": "90",
    "Map name": "random-32-32-20.map",
    "Success": "1",
    "Computation time (ms)": "60",
    "Makespan": "50",
    "Factorized": "Standard",
    "Multi threading": "no",
    "Loop count": "53",
    "PIBT calls": "4677",
    "Active PIBT calls": "2446",
    "Action counts": "5247",
    "Active action counts": "2911",
    "Sum of costs": "2782",
    "Sum of loss": "2782",
    "CPU usage (percent)": "57",
    "Maximum RAM usage (Mbytes)": "5.112",
    "Average RAM usage (Mbytes)": "0.0"
}
"""


def show_plots(map_name: str, update_db: bool) :

    app = Dash(__name__, external_stylesheets=[dbc.themes.BOOTSTRAP])

    # Create a basic color palette
    colors = {
        'background': '#222222',           # #666976, #2d2d30
        'text': '#E9E9E9' 
    }

    # Gather data in specific map
    data, data_success = get_data(map_name, update_db)

    #data_cut = data.groupby(['Number of agents', 'Factorized', 'Maximum RAM usage (Mbytes)', 'Computation time (ms)', 'CPU usage (percent)', 'Multi threading']) #.mean().reset_index()
    data1 = data.drop(data[data['Multi threading'] == "no"].index)      # with MT
    data2 = data.drop(data[data['Multi threading'] == "yes"].index)     # without MT

    # Create the line charts
    line_CPU = px.line(data, x="Number of agents", y="CPU usage (percent)", color="Factorized")
    line_RAM1 = px.line(data1, x="Number of agents", y="Maximum RAM usage (Mbytes)", color="Factorized", labels="Multi threading")
    line_RAM2 = px.line(data2, x="Number of agents", y="Maximum RAM usage (Mbytes)", color="Factorized", labels="Multi threading")
    line_time1 = px.line(data1, x="Number of agents", y="Computation time (ms)", color="Factorized")
    line_time2 = px.line(data2, x="Number of agents", y="Computation time (ms)", color="Factorized")
    
    line_actions = px.line(data, x="Number of agents", y="Average action counts", color="Factorized")
    line_success = px.line(data_success, x="Number of agents", y="Success", color="Factorized")


    line_CPU.update_layout(
        plot_bgcolor=colors['background'],
        paper_bgcolor=colors['background'],
        font_color=colors['text'],
        showlegend=False,
        title_text="CPU usage (percent)",
        title_x=0.5,
        title_xanchor="center",
        xaxis_title="Number of agents",
        yaxis_title=None,
        title=dict(font=dict(size=14)),
        height=260,
        width=450,
    )

    line_RAM1.update_layout(
        plot_bgcolor=colors['background'],
        paper_bgcolor=colors['background'],
        font_color=colors['text'],
        showlegend=True,
        title_text="Maximum RAM usage (Mbytes) with MT",
        title_x=0.5,
        title_xanchor="center",
        xaxis_title="Number of agents",
        yaxis_title=None,
        title=dict(font=dict(size=14)),
        height=260,
        width=480,
    )

    line_RAM2.update_layout(
        plot_bgcolor=colors['background'],
        paper_bgcolor=colors['background'],
        font_color=colors['text'],
        showlegend=True,
        title_text="Maximum RAM usage (Mbytes) no MT",
        title_x=0.5,
        title_xanchor="center",
        xaxis_title="Number of agents",
        yaxis_title=None,
        title=dict(font=dict(size=14)),
        height=260,
        width=480,
    )

    line_time1.update_layout(
        plot_bgcolor=colors['background'],
        paper_bgcolor=colors['background'],
        font_color=colors['text'],
        showlegend=False,
        title_text="Computation time (ms) with MT",
        title_x=0.5,
        title_xanchor="center",
        xaxis_title="Number of agents",
        yaxis_title=None,
        title=dict(font=dict(size=14)),
        height=260,
        width=450,
    )

    line_time2.update_layout(
        plot_bgcolor=colors['background'],
        paper_bgcolor=colors['background'],
        font_color=colors['text'],
        showlegend=False,
        title_text="Computation time (ms) no MT",
        title_x=0.5,
        title_xanchor="center",
        xaxis_title="Number of agents",
        yaxis_title=None,
        title=dict(font=dict(size=14)),
        height=260,
        width=450,
    )

    line_actions.update_layout(
        plot_bgcolor=colors['background'],
        paper_bgcolor=colors['background'],
        font_color=colors['text'],
        showlegend=False,
        title_text="Average action counts",
        title_x=0.5,
        title_xanchor="center",
        xaxis_title="Number of agents",
        yaxis_title=None,
        title=dict(font=dict(size=14)),
        height=260,
        width=480,
    )

    line_success.update_layout(
        plot_bgcolor=colors['background'],
        paper_bgcolor=colors['background'],
        font_color=colors['text'],
        showlegend=False,
        title_text="Successfully solved instances",
        title_x=0.5,
        title_xanchor="center",
        xaxis_title="Number of agents",
        yaxis_title=None,
        title=dict(font=dict(size=14)),
        height=260,
        width=400,
    )


    # Create the bar charts
    bar_success = px.histogram(data_success, x="Factorized", y="Success", color="Factorized", histfunc='sum', text_auto=True, orientation='v', labels=None)
    bar_success.update_layout(
        plot_bgcolor=colors['background'],
        paper_bgcolor=colors['background'],
        font_color=colors['text'],
        showlegend=False,
        title_text="Successfully solved instances",
        title_x=0.5,
        title_xanchor="center",
        xaxis_title=None,
        yaxis_title=None,
        yaxis={'showgrid':False, 'showticklabels':False},
        title=dict(font=dict(size=14)),
        height=280,
        width=400,
    )


    print("\nDashboard updated")

    # Layout of the Dashboard
    app.layout = html.Div(style={'backgroundColor': colors['background']}, children=[
        
        dbc.Row(dbc.Col(html.Div(html.P(["", html.Br(), "Performance overview (", map_name, ")"]))), style=
                {'textAlign': 'center',
                'color': colors['text'],
                'fontSize': 25,
                'height' : '70px',
                }),

        dbc.Row(
            [
                #dbc.Col(html.Div(html.P(["", html.Br(), "Map style tested : ", "random-32-32-20.map"])), width=3,
                #        style={'color': colors['text'],
                #               'fontSize': 24,
                #               'padding-left': '2em'}),
                dbc.Col(dcc.Graph(id='graph1',figure=bar_success), width=3, style={'textAlign': 'center'}),
                dbc.Col(dcc.Graph(id='graph2',figure=line_time1), width=4),
                dbc.Col(dcc.Graph(id='graph3',figure=line_RAM1), width=5)
            ]
        ),
        dbc.Row(
            [
                #dbc.Col(width=3, style={'textAlign': 'center'}),
                dbc.Col(dcc.Graph(id='graph4',figure=line_CPU), width=3),
                dbc.Col(dcc.Graph(id='graph5',figure=line_time2), width=4, style={'textAlign': 'center'}),
                dbc.Col(dcc.Graph(id='graph6',figure=line_RAM2), width=5, style={'textAlign': 'center'})
            ]
        ),

        #dbc.Row(dbc.Col(html.Div("")), style={'height' : '200px'}),

    ])



    if __name__ == '__main__':
        app.run(debug=True)



show_plots(map_name="warehouse-20-40-10-2-2.map", update_db=False)